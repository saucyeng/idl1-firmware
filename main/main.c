/* IDL1 firmware entry point.
 *
 * Boot order (bring-up phase 1): switched-rail power (PWR_EN) first, then
 * the MCU-only subsystems, then the buses and sensors on the switched rail,
 * then BLE, and finally the one-block boot self-test report (bringup.c).
 * BLE control commands are dispatched to on_ble_command() below.
 */

#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ble_service.h"
#include "button.h"
#include "board_leds.h"
#include "bringup.h"
#include "device_id.h"
#include "diag_log.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "gps_task.h"
#include "hrm_task.h"
#include "i2c_bus.h"
#include "idl0_config.h"
#include "imu_task.h"
#include "led_status.h"
#include "mode_state.h"
#include "ota.h"
#include "pins.h"
#include "sd_logger.h"
#include "writer_task.h"
#include "status.h"
#include "session.h"
#include "tla2528.h"
#include "wifi_server.h"

static const char *TAG = "idl0";

/* The tla2528 component carries its own copy of the board's channel wiring
 * (it cannot include pins.h); keep the two in lockstep. */
_Static_assert(TLA2528_CH_LED_RED == IDL1_TLA_CH_LED_RED, "TLA red LED channel");
_Static_assert(TLA2528_CH_LED_GREEN == IDL1_TLA_CH_LED_GREEN, "TLA green LED channel");
_Static_assert(TLA2528_CH_ANA_R == IDL1_TLA_CH_ANA_R, "TLA rear analog channel");
_Static_assert(TLA2528_CH_ANA_F == IDL1_TLA_CH_ANA_F, "TLA front analog channel");

/* Invoked from the BLE host task when a control command is written. */
static uint8_t on_ble_command(idl0_ble_command_t cmd)
{
    uint8_t ack = IDL0_ACK_OK;
    switch (cmd) {
        case IDL0_CMD_START_LOGGING:
            ESP_LOGI(TAG, "command: START_LOGGING");
            ack = idl0_session_on_ble_command(cmd);
            break;
        case IDL0_CMD_STOP_LOGGING:
            ESP_LOGI(TAG, "command: STOP_LOGGING");
            ack = idl0_session_on_ble_command(cmd);
            break;
        case IDL0_CMD_WIFI_ON:
            ESP_LOGI(TAG, "command: WIFI_ON");
            if (idl0_session_is_running()) {
                ESP_LOGW(TAG, "WIFI_ON refused: session running (mutex \302\2477.2)");
                ack = IDL0_ACK_MUTEX_REFUSED;
                break;
            }
            idl0_wifi_start();
            break;
        case IDL0_CMD_WIFI_OFF:
            ESP_LOGI(TAG, "command: WIFI_OFF");
            /* T11: this callback runs on the NimBLE host task. A direct
             * idl0_wifi_stop() blocks it on httpd_stop() joining the
             * httpd worker for as long as any in-flight handler takes,
             * wedging all BLE processing meanwhile — defer it instead
             * (same timer machinery as POST /wifi_off). The ack stays
             * IDL0_ACK_OK: the stop is committed, just asynchronous —
             * idl0_wifi_stop_deferred() cannot fail in a way the caller
             * needs to see (it logs and no-ops on the rare timer-arm
             * error, matching idl0_wifi_stop's own idempotence). */
            idl0_wifi_stop_deferred();
            break;
        case IDL0_CMD_CALIBRATE_IMU:
            ESP_LOGI(TAG, "command: CALIBRATE_IMU (no-op until IMU bring-up)");
            // TODO(idl0): real calibration flow (spec §7.6)
            ack = IDL0_ACK_NOT_IMPLEMENTED;  /* T15: no-op, not success */
            break;
        case IDL0_CMD_OTA_CONFIRM:
            ESP_LOGI(TAG, "command: OTA_CONFIRM");
            if (!idl0_ota_mark_valid()) {
                ESP_LOGW(TAG, "OTA_CONFIRM with no pending-verify image");
            }
            break;
        case IDL0_CMD_POWER_OFF:
            ESP_LOGI(TAG, "command: POWER_OFF");
            idl1_power_off_request();
            break;
        case IDL0_CMD_CONFIG_BEGIN:
        case IDL0_CMD_CONFIG_COMMIT:
        case IDL0_CMD_CONFIG_READ_BEGIN:
        case IDL0_CMD_FILES_READ_BEGIN:
            /* Config push/read handshakes are handled entirely in the BLE
             * layer (it owns the FF05/FF06 buffers) and never routed here.
             * Defensive: if one ever reaches the dispatcher, refuse cleanly. */
            ESP_LOGW(TAG, "config command 0x%02X reached dispatcher", cmd);
            ack = IDL0_ACK_NOT_IMPLEMENTED;
            break;
    }
    idl0_status_publish();
    return ack;
}

/* Settle time after PWR_EN goes high before anything on the switched rail
 * is touched, ms. Rounded envelope over: TPS22919 rail rise, LSM6DSV320X
 * boot, gyro turn-on 40 ms, ATS601 <=1 ms, TLA2528 tPU <=5 ms
 * (pod-firmware-plan.md PT17). A boot-sequencing constant, not a sensor
 * setting. */
#define IDL1_PWR_EN_SETTLE_MS 50

/* Time the switched rail is held off at every boot before it comes up, ms.
 * The TPS22919's 24 ohm quick output discharge empties the rail in a few
 * ms; the rest is margin so every chip on it sees a full power-on reset. */
#define IDL1_PWR_EN_OFF_MS 100

/* Shutdown hook: drop the switched rail before esp_restart() resets the
 * MCU, so the rail goes off cleanly instead of floating (below). */
static void idl1_power_rail_off_at_restart(void)
{
    gpio_set_level(IDL1_PIN_PWR_EN, 0);
}

/* Power-cycle the switched rail, then drive PWR_EN high and leave it there
 * for the life of the firmware.
 *
 * U2 (TPS22919) switches +3V3 to every peripheral: GNSS, onboard IMU, SD,
 * TLA2528 (and both board LEDs), both pods, Qwiic, and the I2C pull-ups
 * R8/R9. Its ON input is a non-latching level with no pull on the net, so
 * GPIO2 must be an actively driven output from here on. Nothing else in the
 * firmware may reconfigure IDL1_PIN_PWR_EN — no gpio_reset_pin(), no
 * direction change — or the whole board goes dark while the MCU keeps
 * running.
 *
 * Why the rail is cycled first: across a software reset (OTA, config
 * apply, panic) GPIO2 is released and ON floats for most of a second, so
 * the rail sags or glitches rather than switching off. The chips on it then
 * come back from a brownout, not a power-on: logged on 2026-10-19 after the
 * beta.12 OTA reboot, the TLA2528 (0x10) and the front pod (0x6A) both
 * stopped answering while the rear pod (0x6B) came back. The TLA2528 also
 * decodes ADDR only at power-up (NT1, docs/bringup.md). Holding the rail
 * off for IDL1_PWR_EN_OFF_MS makes every boot a real power-on for them.
 * Nothing drives into the rail meanwhile: after reset every MCU pin is an
 * input, and the I2C pull-ups are on the rail itself.
 *
 * Sanctioned exceptions to "never touch PWR_EN": this cycle, the restart
 * hook above, and the long-press power-off in button.c (low, held through
 * deep sleep).
 *
 * INPUT_OUTPUT rather than OUTPUT keeps the input buffer on so the boot
 * self-test can read the pad back; it does not change what the pin
 * drives. */
static void idl1_power_rail_on(void)
{
    gpio_set_level(IDL1_PIN_PWR_EN, 0);
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << IDL1_PIN_PWR_EN,
        .mode         = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(IDL1_PIN_PWR_EN, 0);
    vTaskDelay(pdMS_TO_TICKS(IDL1_PWR_EN_OFF_MS));
    gpio_set_level(IDL1_PIN_PWR_EN, 1);
    esp_register_shutdown_handler(idl1_power_rail_off_at_restart);
}

/* Log every address that ACKs on the shared I2C bus, once at boot. Read-only
 * probes, a few ms in all; tells a wedged chip (answers nowhere) from a
 * mis-decoded TLA2528 ADDR (answers at 0x11-0x17). */
static void idl1_i2c_scan_log(i2c_master_bus_handle_t bus)
{
    char line[96];
    int  len = 0;
    line[0] = '\0';
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        if (i2c_master_probe(bus, addr, 20) == ESP_OK && len < (int)sizeof(line) - 6) {
            len += snprintf(line + len, sizeof(line) - len, " 0x%02X", addr);
        }
    }
    ESP_LOGI(TAG, "I2C devices answering:%s", len ? line : " none");
}

void app_main(void)
{
    /* First statement: power the switched rail. Everything that drives a
     * line into a switched-rail chip (SPI chip-selects, SPI/I2C buses, the
     * GPS UART) must wait for the settle below. Driving a CS high into an
     * unpowered IMU/SD back-powers it through its input clamp diodes
     * (bring-up correction R7), so the order is PWR_EN -> settle -> CS idle
     * -> buses. */
    idl1_button_release_sleep_holds();   /* button power-off holds PWR_EN low */
    idl1_power_rail_on();
    vTaskDelay(pdMS_TO_TICKS(IDL1_PWR_EN_SETTLE_MS));

    ESP_LOGI(TAG, "IDL1 firmware boot");

    idl0_device_id_init();

    /* Latch the running OTA partition's pending-verify state before any
     * other subsystem starts. Do NOT auto-mark-valid — the app must
     * send CMD_OTA_CONFIRM after it has verified the device is healthy.
     * If we never get that confirmation and reboot, the bootloader
     * rolls back to the previous slot. */
    idl0_ota_init();

    /* T17: create the mode event group once, here, before any task or
     * subsystem that could call idl0_mode_set_bits/_clear_bits/_get_bits or
     * idl0_mode_event_group() starts — removes the old lazy check-then-act
     * create race on first concurrent callers. */
    idl0_mode_init();

    if (!idl0_led_init()) {
        /* T16: the LED is non-essential — halting here forever meant the
         * device never reset, so a bad OTA image was never rolled back and
         * BLE/WiFi never came up. Log and continue booting instead. */
        ESP_LOGE(TAG, "LED init failed; continuing without status LED");
    }
    idl0_led_set(IDL0_LED_BLINK_SLOW);

    /* Idle every SPI chip-select BEFORE bringing up the shared SPI bus, so
     * no device reads a floating CS as asserted and drives MISO during SD
     * card init (on IDL0 that corrupted the SCR read and failed the mount
     * with ESP_ERR_INVALID_RESPONSE). On IDL1 only the onboard IMU0 and the
     * SD card sit on SPI; IMU0's CS is later re-claimed by the spi_master
     * driver via spics_io_num, so explicit pre-idling is harmless. */
    static const gpio_num_t cs_pins[] = {
        IDL1_PIN_CS_IMU0, IDL1_PIN_CS_SD,
    };
    for (size_t i = 0; i < sizeof(cs_pins) / sizeof(cs_pins[0]); i++) {
        gpio_reset_pin(cs_pins[i]);
        gpio_set_direction(cs_pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(cs_pins[i], 1);   /* deasserted */
    }

    if (!idl0_sd_init(SPI2_HOST)) {
        ESP_LOGW(TAG, "SD card not available at boot");
    }

    if (!idl0_writer_init()) {
        ESP_LOGE(TAG, "writer init failed; logging will not work");
    }

    /* Shared I2C bus (TLA2528 + both pods), then the TLA2528 at its one
     * expected address. Boot continues whatever happens here (H2): a silent
     * TLA2528 only costs the analog inputs and the two board LEDs. */
    i2c_master_bus_handle_t i2c_bus = idl1_i2c_bus_get();
    if (i2c_bus == NULL) {
        ESP_LOGE(TAG, "I2C bus init failed; pods and TLA2528 unavailable");
    } else {
        idl1_i2c_scan_log(i2c_bus);
    }
    if (tla2528_init(i2c_bus, IDL1_I2C_ADDR_TLA)) {
        /* Green = power (H1) from here; board_leds takes over the roles once
         * BLE is up (below). */
        tla2528_led_set(false, true);
    }

    if (!idl0_imu_task_start()) {
        ESP_LOGW(TAG, "IMU unavailable — sessions will record without IMU");
    }

    if (!idl0_gps_task_start()) {
        ESP_LOGW(TAG, "GPS unavailable — sessions will record without GPS");
    }

    if (!idl0_ble_init(on_ble_command)) {
        ESP_LOGE(TAG, "BLE init failed; LED only");
    } else {
        ESP_LOGI(TAG, "BLE control plane up as %s", idl0_device_name());
        idl0_status_publish();
        idl0_status_publisher_start();
    }

    /* Board LED roles (H1): green = power + wireless, red = recording. */
    idl1_board_leds_start();

    /* HRM task picks up the saved address from idl0_config.json. NimBLE
     * is already initialised by idl0_ble_init above; the central role
     * uses the same host instance. No-op if HRM is disabled. */
    {
        idl0_config_t cfg;
        idl0_config_load(&cfg);
        hrm_task_start(&cfg.hrm);
    }

    /* On-SD diagnostics (heap trend + brownout-reset trail) to chase the
     * long-session WiFi degradation. No-op without SD; skips itself during a
     * logging session so it never contends with session writes. */
    idl0_diag_log_init();

    /* Last: the one-block boot self-test report (bring-up phase 1). Runs
     * after everything above so it reports their boot results; takes
     * IDL1_BRINGUP_WINDOW_MS to watch GPS traffic. */
    idl1_bringup_report();
    idl1_board_leds_set_booted();

    /* After the self-test, which samples USER_BTN itself. */
    idl1_button_start();

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
