#include "status.h"

#include <stdio.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "battery.h"
#include "ble_service.h"
#include "gps_driver.h"
#include "hrm_task.h"
#include "imu_driver.h"
#include "imu_task.h"
#include "ota.h"
#include "sd_logger.h"
#include "session.h"
#include "wifi_server.h"
#include "writer_task.h"

static const char *TAG = "status";

static esp_timer_handle_t s_publish_timer = NULL;

/* Sized for the full §7.3 line set with every variable-length token at its
 * max: WiFi, Logging, LoggingElapsed (10 digits), BatteryRaw + BatteryMv,
 * SD, SDFreeMiB (10 digits), GPS, GPSFix, GPSSats, IMU, IMU0-IMU2, Firmware
 * (31), OTA, HR, HR_Battery: about 330 bytes. 384 leaves headroom; the
 * preferred ATT MTU is 512 (sdkconfig.defaults) so one notification carries
 * it. Keep in lockstep with s_status in ble_service.c. */
#define IDL0_STATUS_BUF_BYTES 384

/* §7.3 per-IMU token for IMU `index`: OFF not configured, ABSENT no answer
 * on the bus, OK delivering data, ERROR answered but not delivering. */
static const char *imu_token(uint8_t index)
{
    const idl0_imu_probe_t probe = idl0_imu_task_probe(index);
    switch (probe.status) {
        case IDL0_IMU_PROBE_NOT_RUN:     return "OFF";
        case IDL0_IMU_PROBE_NO_RESPONSE: return "ABSENT";
        case IDL0_IMU_PROBE_ANSWERED:
        default:
            return idl0_imu_task_is_active(index) ? "OK" : "ERROR";
    }
}

void idl0_status_publish(void)
{
    char status[IDL0_STATUS_BUF_BYTES];
    const idl0_sd_state_t sd_state = idl0_sd_state();

    int n = snprintf(status, sizeof(status),
             "WiFi: %s\nLogging: %s",
             idl0_wifi_state_str(idl0_wifi_state()),
             idl0_session_is_running() ? "RUNNING" : "STOPPED");
    if (n < 0) return;
    size_t cursor = (size_t)n;

    if (cursor < sizeof(status) && idl0_session_is_running()) {
        int add = snprintf(status + cursor, sizeof(status) - cursor,
                           "\nLoggingElapsed: %lu", (unsigned long)idl0_session_elapsed_s());
        if (add > 0) cursor += (size_t)add;
    }

    /* Battery (§7.3): BatteryRaw is the averaged ADC count, BatteryMv the
     * calibrated cell voltage after the divider (mV), which the app
     * prefers. "Battery: N%" is deprecated and kept one revision for older
     * apps, now estimated from the voltage instead of a hard-coded 100%.
     * All three are left out when the read fails. */
    int bat_raw = 0, bat_pin_mv = 0, bat_mv = 0;
    const char *bat_why = NULL;
    if (cursor < sizeof(status) && idl1_battery_read(&bat_raw, &bat_pin_mv, &bat_mv, &bat_why)) {
        int add = snprintf(status + cursor, sizeof(status) - cursor,
                           "\nBattery: %d%%\nBatteryRaw: %d\nBatteryMv: %d",
                           idl1_battery_pct_estimate(bat_mv), bat_raw, bat_mv);
        if (add > 0) cursor += (size_t)add;
    }

    if (cursor < sizeof(status)) {
        int add = snprintf(status + cursor, sizeof(status) - cursor,
                           "\nSD: %s", idl0_sd_state_str(sd_state));
        if (add > 0) cursor += (size_t)add;
    }

    /* A7 (decisions, narrowed scope) / R2: card capacity + usage, directly
     * after SD: in the emit order. Omitted entirely when ABSENT (no card to
     * report on) rather than printing 0/0 — §7.3 says unknown lines are
     * ignored by the app, so adding these is additive-safe either way.
     * Values come from sd_logger's cache (idl0_sd_free_mb/idl0_sd_total_mb)
     * — never a live FATFS query here, since this runs at 1 Hz whether or
     * not a session is active (see sd_logger.c for the refresh points). */
    /* §7.3 SDFreeMiB, present for SD OK|FULL only. sd_logger's "MB" are
     * MiB (bytes / 1024 / 1024). Replaces SD_Free/SD_Total, which matched
     * no app parser. */
    if (cursor < sizeof(status) && (sd_state == IDL0_SD_OK || sd_state == IDL0_SD_FULL)) {
        int add = snprintf(status + cursor, sizeof(status) - cursor,
                           "\nSDFreeMiB: %u", (unsigned)idl0_sd_free_mb());
        if (add > 0) cursor += (size_t)add;
    }

    /* T32: idl0_imu_task_state() (imu_task.c) aggregates both buses — SPI
     * (IMU0) and I²C (IMU1/IMU2) — unlike idl0_imu_state() (imu_driver.c),
     * which only sees the SPI driver and would otherwise misreport "OK"
     * while both I²C IMUs are down. */
    if (cursor < sizeof(status)) {
        int add = snprintf(status + cursor, sizeof(status) - cursor,
                           "\nGPS: %s", idl0_gps_state_str(idl0_gps_state()));
        if (add > 0) cursor += (size_t)add;
    }
    uint8_t gps_fix = 0, gps_sats = 0;
    if (cursor < sizeof(status) && idl0_gps_live(&gps_fix, &gps_sats)) {
        int add = snprintf(status + cursor, sizeof(status) - cursor,
                           "\nGPSFix: %u\nGPSSats: %u", (unsigned)gps_fix, (unsigned)gps_sats);
        if (add > 0) cursor += (size_t)add;
    }
    if (cursor < sizeof(status)) {
        int add = snprintf(status + cursor, sizeof(status) - cursor,
                           "\nIMU: %s\nIMU0: %s\nIMU1: %s\nIMU2: %s\nFirmware: %s",
                           idl0_imu_state_str(idl0_imu_task_state()),
                           imu_token(0), imu_token(1), imu_token(2),
                           esp_app_get_description()->version);
        if (add > 0) cursor += (size_t)add;
    }

    if (cursor < sizeof(status) && idl0_ota_pending_verify()) {
        int add = snprintf(status + cursor, sizeof(status) - cursor,
                           "\nOTA: PENDING_VERIFY");
        if (add > 0) cursor += (size_t)add;
    }

    /* §7.3 HR line. State mapping:
     *   OFF                          → ABSENT
     *   STREAMING + contact ok       → CONNECTED <bpm>
     *   STREAMING + no contact       → NO_CONTACT <bpm>
     *   SUSPENDED                    → SUSPENDED
     *   SCANNING / CONNECTING / DISCOVERING → SEARCHING */
    const hrm_state_t hr_state = hrm_task_state();
    if (cursor < sizeof(status)) {
        int add = 0;
        switch (hr_state) {
            case HRM_STATE_OFF:
                add = snprintf(status + cursor, sizeof(status) - cursor,
                               "\nHR: ABSENT");
                break;
            case HRM_STATE_STREAMING:
                add = snprintf(status + cursor, sizeof(status) - cursor,
                               "\nHR: %s %u",
                               hrm_task_no_contact() ? "NO_CONTACT" : "CONNECTED",
                               (unsigned)hrm_task_latest_bpm());
                break;
            case HRM_STATE_SUSPENDED:
                add = snprintf(status + cursor, sizeof(status) - cursor,
                               "\nHR: SUSPENDED");
                break;
            case HRM_STATE_SCANNING:
            case HRM_STATE_CONNECTING:
            case HRM_STATE_DISCOVERING:
            default:
                add = snprintf(status + cursor, sizeof(status) - cursor,
                               "\nHR: SEARCHING");
                break;
        }
        if (add > 0) cursor += (size_t)add;
    }

    if (cursor < sizeof(status) && hrm_task_has_battery()) {
        snprintf(status + cursor, sizeof(status) - cursor,
                 "\nHR_Battery: %u%%", (unsigned)hrm_task_battery_pct());
    }

    idl0_ble_publish_status(status);
}

static void publish_timer_cb(void *arg)
{
    (void)arg;
    /* T04 mechanism, half 2 (§10.2 / A7 ruling, verbatim): SD FULL/ERROR
     * mid-session must stop the session and report it over BLE — no new
     * task, the 1 Hz status timer does both. Checked here, before building
     * the status string, so the same publish below already reflects the
     * post-stop state ("SD: FULL|ERROR" / "Logging: STOPPED").
     * idl0_session_stop()'s bounded drain loop is safe to run on the
     * esp_timer task: the writer keeps consuming queued items even while
     * idl0_sd_append is failing (it just counts the failure and moves on),
     * so the ring buffer empties well inside the drain's cap rather than
     * timing out and blocking this task. */
    if (idl0_writer_sd_fault() && idl0_session_is_running()) {
        idl0_session_stop();
    }
    idl0_status_publish();
}

void idl0_status_publisher_start(void)
{
    if (s_publish_timer != NULL) return;
    const esp_timer_create_args_t args = {
        .callback = publish_timer_cb,
        .arg      = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "status_pub",
        .skip_unhandled_events = true,
    };
    esp_err_t err = esp_timer_create(&args, &s_publish_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_timer_create: %s", esp_err_to_name(err));
        return;
    }
    /* 1 Hz — keeps BPM, NO_CONTACT, and any state we don't explicitly
     * hook fresh on the FF04 characteristic at ~1 s lag worst case. */
    err = esp_timer_start_periodic(s_publish_timer, 1000 * 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_timer_start_periodic: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "publisher started at 1 Hz");
}
