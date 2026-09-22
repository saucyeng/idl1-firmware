/* See bringup.h. */

#include "bringup.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "diag_log.h"
#include "gps_driver.h"
#include "gps_task.h"
#include "imu_task.h"
#include "pins.h"
#include "sd_logger.h"
#include "tla2528.h"

static const char *TAG = "bringup";

/* Battery divider (pins.h): R2 = 1M from BAT+, R3 = 2M to GND, so
 * Vbat = Vpin * 3 / 2. */
#define BAT_DIV_NUM 3
#define BAT_DIV_DEN 2
/* Plausible single-cell Li-ion window at the battery, mV. Below
 * BAT_ABSENT_MV the divider sees essentially nothing (no cell, open
 * divider). */
#define BAT_MIN_MV    3000
#define BAT_MAX_MV    4350
#define BAT_ABSENT_MV 500
/* ADC samples averaged for the one battery reading, count. */
#define BAT_SAMPLES   16

/* USER_BTN poll period during the report window, ms. */
#define BTN_POLL_MS   20

typedef enum { ROW_PASS, ROW_FAIL, ROW_ABSENT } row_t;

static const char *row_str(row_t r)
{
    switch (r) {
        case ROW_PASS: return "PASS  ";
        case ROW_FAIL: return "FAIL  ";
        default:       return "ABSENT";
    }
}

static int s_pass, s_fail, s_absent;

static void row(const char *name, row_t r, const char *detail)
{
    if (r == ROW_PASS)      s_pass++;
    else if (r == ROW_FAIL) s_fail++;
    else                    s_absent++;
    ESP_LOGI(TAG, "| %-12s %s %s", name, row_str(r), detail);
}

/* ---- IMU rows ------------------------------------------------------------ */

/* Classifies one IMU's WHO_AM_I probe. `on_i2c` pods may legitimately be
 * unplugged (ABSENT); the onboard SPI IMU may not. Writes a short token for
 * the diag line into tok ("73", "--", "nr", "ff", ...). */
static row_t imu_row(uint8_t idx, bool on_i2c, char *detail, size_t dlen,
                     char *tok, size_t tlen)
{
    const idl0_imu_probe_t p = idl0_imu_task_probe(idx);
    switch (p.status) {
        case IDL0_IMU_PROBE_NOT_RUN:
            snprintf(detail, dlen, "not probed (IMU%u disabled in config)",
                     (unsigned)idx);
            snprintf(tok, tlen, "off");
            return ROW_ABSENT;
        case IDL0_IMU_PROBE_NO_RESPONSE:
            snprintf(tok, tlen, "--");
            if (on_i2c) {
                snprintf(detail, dlen, "no ACK (pod unplugged? check M8 cable, JP1)");
                return ROW_ABSENT;
            }
            snprintf(detail, dlen, "no response (SPI transaction failed)");
            return ROW_FAIL;
        case IDL0_IMU_PROBE_ANSWERED:
        default:
            break;
    }
    snprintf(tok, tlen, "%02x", p.who_am_i);
    if (p.who_am_i == IDL1_IMU_WHO_AM_I_DSV320X) {
        if (idl0_imu_task_is_active(idx)) {
            snprintf(detail, dlen, "WHO_AM_I=0x%02X LSM6DSV320X, configured and running",
                     p.who_am_i);
            return ROW_PASS;
        }
        snprintf(detail, dlen, "WHO_AM_I=0x%02X LSM6DSV320X but configuration FAILED (see log)",
                 p.who_am_i);
        return ROW_FAIL;
    }
    if (!on_i2c && (p.who_am_i == 0x00 || p.who_am_i == 0xFF)) {
        snprintf(detail, dlen, "WHO_AM_I=0x%02X: nothing driving MISO (chip unpowered / CS?)",
                 p.who_am_i);
        return ROW_FAIL;
    }
    snprintf(detail, dlen, "WHO_AM_I=0x%02X unexpected (want 0x%02X)",
             p.who_am_i, IDL1_IMU_WHO_AM_I_DSV320X);
    return ROW_FAIL;
}

/* ---- Battery ------------------------------------------------------------- */

/* One averaged, calibrated BAT_SENSE reading. Returns false (with *why set)
 * if the ADC or its calibration is unavailable. The unit is released again
 * afterwards: battery monitoring proper is phase 3. */
static bool read_battery(int *pin_mv, int *raw_avg, const char **why)
{
    adc_unit_t unit;
    adc_channel_t chan;
    if (adc_oneshot_io_to_channel(IDL1_PIN_BAT_SENSE, &unit, &chan) != ESP_OK) {
        *why = "GPIO is not an ADC pin";
        return false;
    }
    adc_oneshot_unit_handle_t adc = NULL;
    const adc_oneshot_unit_init_cfg_t ucfg = { .unit_id = unit };
    if (adc_oneshot_new_unit(&ucfg, &adc) != ESP_OK) {
        *why = "adc_oneshot_new_unit failed";
        return false;
    }
    /* 12 dB attenuation: full scale ~3.1 V at the pin, above the 2.8 V a
     * full cell puts there. */
    const adc_oneshot_chan_cfg_t ccfg = {
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    bool ok = false;
    adc_cali_handle_t cali = NULL;
    if (adc_oneshot_config_channel(adc, chan, &ccfg) != ESP_OK) {
        *why = "adc_oneshot_config_channel failed";
        goto out;
    }
    int32_t sum = 0;
    for (int i = 0; i < BAT_SAMPLES; i++) {
        int raw = 0;
        if (adc_oneshot_read(adc, chan, &raw) != ESP_OK) {
            *why = "adc_oneshot_read failed";
            goto out;
        }
        sum += raw;
    }
    *raw_avg = (int)(sum / BAT_SAMPLES);

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    const adc_cali_curve_fitting_config_t kcfg = {
        .unit_id  = unit,
        .chan     = chan,
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&kcfg, &cali) != ESP_OK) {
        cali = NULL;
        *why = "no ADC calibration (eFuse not burnt?)";
        goto out;
    }
    if (adc_cali_raw_to_voltage(cali, *raw_avg, pin_mv) != ESP_OK) {
        *why = "adc_cali_raw_to_voltage failed";
        goto out;
    }
    ok = true;
#else
    *why = "no curve-fitting calibration on this target";
#endif

out:
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (cali != NULL) adc_cali_delete_scheme_curve_fitting(cali);
#endif
    adc_oneshot_del_unit(adc);
    return ok;
}

/* ---- Report -------------------------------------------------------------- */

void idl1_bringup_report(void)
{
    char d[160];
    char name[16];
    s_pass = s_fail = s_absent = 0;

    /* USER_BTN has no driver yet: plain input, internal pull-up, active-low
     * (pins.h). Configured here so the window below can watch it. */
    const gpio_config_t btn_cfg = {
        .pin_bit_mask = 1ULL << IDL1_PIN_USER_BTN,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    const bool btn_cfg_ok = gpio_config(&btn_cfg) == ESP_OK;
    vTaskDelay(pdMS_TO_TICKS(BTN_POLL_MS));   /* let the pull-up settle */
    const int btn_start = gpio_get_level(IDL1_PIN_USER_BTN);

    /* Observation window: GPS traffic and any button press. */
    const uint32_t gps_b0 = gps_task_diag_rx_bytes();
    const uint32_t gps_s0 = gps_task_diag_rx_sentences();
    bool btn_press_seen = false;
    for (int t = 0; t < IDL1_BRINGUP_WINDOW_MS; t += BTN_POLL_MS) {
        vTaskDelay(pdMS_TO_TICKS(BTN_POLL_MS));
        if (gpio_get_level(IDL1_PIN_USER_BTN) == 0) btn_press_seen = true;
    }
    const uint32_t gps_bytes = gps_task_diag_rx_bytes() - gps_b0;
    const uint32_t gps_nmea  = gps_task_diag_rx_sentences() - gps_s0;

    ESP_LOGI(TAG, "================ IDL1 BOOT SELF-TEST ================");

    /* PWR_EN: pad read back (main.c configures it input+output). */
    const int pwr = gpio_get_level(IDL1_PIN_PWR_EN);
    snprintf(d, sizeof(d), "GPIO%d reads %d (%s)", IDL1_PIN_PWR_EN, pwr,
             pwr ? "switched 3V3 rail enabled" : "RAIL OFF: every peripheral unpowered");
    row("PWR_EN", pwr ? ROW_PASS : ROW_FAIL, d);

    /* SD */
    const idl0_sd_state_t sd = idl0_sd_state();
    const bool sd_mounted = (sd == IDL0_SD_OK || sd == IDL0_SD_FULL);
    if (sd_mounted) {
        snprintf(d, sizeof(d), "mounted, %lu MB total, %lu MB free (%s)",
                 (unsigned long)idl0_sd_total_mb(), (unsigned long)idl0_sd_free_mb(),
                 idl0_sd_state_str(sd));
    } else {
        snprintf(d, sizeof(d), "not mounted (state %s: no card / unformatted / CS?)",
                 idl0_sd_state_str(sd));
    }
    row("SD", sd_mounted ? ROW_PASS : ROW_FAIL, d);

    /* IMUs */
    char tok_imu0[4], tok_podf[4], tok_podr[4];
    row_t r = imu_row(0, false, d, sizeof(d), tok_imu0, sizeof(tok_imu0));
    row("IMU0 SPI", r, d);
    r = imu_row(1, true, d, sizeof(d), tok_podf, sizeof(tok_podf));
    snprintf(name, sizeof(name), "POD F 0x%02X", IDL1_I2C_ADDR_IMU_FRONT);
    row(name, r, d);
    r = imu_row(2, true, d, sizeof(d), tok_podr, sizeof(tok_podr));
    snprintf(name, sizeof(name), "POD R 0x%02X", IDL1_I2C_ADDR_IMU_REAR);
    row(name, r, d);

    /* TLA2528 */
    tla2528_probe_t tla;
    tla2528_get_probe(&tla);
    char tok_tla[4];
    if (!tla.probed) {
        snprintf(d, sizeof(d), "not probed");
        snprintf(tok_tla, sizeof(tok_tla), "np");
        row("TLA2528", ROW_FAIL, d);
    } else if (!tla.acked) {
        snprintf(d, sizeof(d), "no ACK at 0x%02X: board LEDs + analog off (ADDR/NT1, see bringup.md)",
                 tla.addr);
        snprintf(tok_tla, sizeof(tok_tla), "--");
        row("TLA2528", ROW_FAIL, d);
    } else {
        snprintf(tok_tla, sizeof(tok_tla), "%02x", tla.system_status);
        if (!tla.status_id_ok) {
            snprintf(d, sizeof(d), "ACK at 0x%02X but SYSTEM_STATUS=0x%02X fails reserved-bit check",
                     tla.addr, tla.system_status);
            row("TLA2528", ROW_FAIL, d);
        } else if (!tla.configured) {
            snprintf(d, sizeof(d), "ACK at 0x%02X, SYSTEM_STATUS=0x%02X, LED config FAILED",
                     tla.addr, tla.system_status);
            row("TLA2528", ROW_FAIL, d);
        } else {
            snprintf(d, sizeof(d), "ACK at 0x%02X, SYSTEM_STATUS=0x%02X, green LED on",
                     tla.addr, tla.system_status);
            row("TLA2528", ROW_PASS, d);
        }
    }

    /* Battery */
    int pin_mv = 0, raw = 0, vbat_mv = -1;
    const char *why = "";
    if (read_battery(&pin_mv, &raw, &why)) {
        vbat_mv = pin_mv * BAT_DIV_NUM / BAT_DIV_DEN;
        snprintf(d, sizeof(d), "pin %d mV (raw %d) x1.5 -> Vbat %d mV", pin_mv, raw, vbat_mv);
        if (vbat_mv < BAT_ABSENT_MV) {
            row("BATTERY", ROW_ABSENT, d);
        } else if (vbat_mv < BAT_MIN_MV || vbat_mv > BAT_MAX_MV) {
            row("BATTERY", ROW_FAIL, d);
        } else {
            row("BATTERY", ROW_PASS, d);
        }
    } else {
        snprintf(d, sizeof(d), "%s (raw %d)", why, raw);
        row("BATTERY", ROW_FAIL, d);
    }

    /* USER_BTN */
    if (!btn_cfg_ok) {
        row("USER_BTN", ROW_FAIL, "gpio_config failed");
    } else {
        snprintf(d, sizeof(d), "GPIO%d=%d at start (%s); press seen in %d ms window: %s",
                 IDL1_PIN_USER_BTN, btn_start,
                 btn_start ? "released" : "LOW: held or shorted",
                 IDL1_BRINGUP_WINDOW_MS, btn_press_seen ? "yes" : "no");
        row("USER_BTN", btn_start ? ROW_PASS : ROW_FAIL, d);
    }

    /* GPS */
    if (idl0_gps_state() == IDL0_GPS_ABSENT) {
        row("GPS", ROW_FAIL, "UART not initialised");
    } else {
        snprintf(d, sizeof(d), "%lu bytes, %lu NMEA lines in %d ms (state %s)",
                 (unsigned long)gps_bytes, (unsigned long)gps_nmea,
                 IDL1_BRINGUP_WINDOW_MS, idl0_gps_state_str(idl0_gps_state()));
        row("GPS", gps_nmea > 0 ? ROW_PASS : ROW_FAIL, d);
    }

    ESP_LOGI(TAG, "---------------- %d PASS  %d FAIL  %d ABSENT ----------------",
             s_pass, s_fail, s_absent);
    ESP_LOGI(TAG, "=====================================================");

    /* One condensed line for the on-SD diag log (no-op without SD). Fits
     * the diag log's 128-char message budget. */
    idl0_diag_log_event(
        "selftest p%d f%d a%d pwr=%d sd=%luMB imu0=%s podF=%s podR=%s tla@%02x=%s "
        "vbat=%dmV btn=%d gps=%lu",
        s_pass, s_fail, s_absent, pwr,
        sd_mounted ? (unsigned long)idl0_sd_total_mb() : 0UL,
        tok_imu0, tok_podf, tok_podr, tla.addr, tok_tla,
        vbat_mv, btn_start, (unsigned long)gps_nmea);
}
