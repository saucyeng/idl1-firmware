#include "status.h"

#include <stdio.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "ble_service.h"
#include "gps_driver.h"
#include "hrm_task.h"
#include "imu_driver.h"
#include "ota.h"
#include "sd_logger.h"
#include "session.h"
#include "wifi_server.h"
#include "writer_task.h"

static const char *TAG = "status";

static esp_timer_handle_t s_publish_timer = NULL;

/* Sized for the §7.3 eleven-line worst case (six base + SD_Free + SD_Total
 * + OTA + HR + HR_Battery — the two SD capacity lines only appear together
 * with a non-ABSENT SD: token, per A7). Recomputed field-by-field with every
 * variable-length token at its max (WiFi 3 "OFF", Logging 7 "RUNNING", SD
 * 5 "ERROR", SD_Free/SD_Total 10 digits each — full uint32_t range, GPS 6
 * "ABSENT", IMU 7 "PARTIAL", Firmware 31 = esp_app_desc_t.version[32] minus
 * the NUL, OTA fixed 19 "OTA: PENDING_VERIFY", HR 18 "HR: NO_CONTACT 255",
 * HR_Battery 16 "HR_Battery: 255%"): 220 bytes + NUL = 221. 256 leaves 35
 * bytes of headroom and matches the BLE preferred MTU (§6 —
 * CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU=256). Keep in lockstep with s_status
 * in ble_service.c. */
#define IDL0_STATUS_BUF_BYTES 256

void idl0_status_publish(void)
{
    char status[IDL0_STATUS_BUF_BYTES];
    const idl0_sd_state_t sd_state = idl0_sd_state();

    /* TODO(idl0): pull Battery from the battery driver (§10.1) once
     * that subsystem lands. */
    int n = snprintf(status, sizeof(status),
             "WiFi: %s\nLogging: %s\nBattery: 100%%\nSD: %s",
             idl0_wifi_state_str(idl0_wifi_state()),
             idl0_session_is_running() ? "RUNNING" : "STOPPED",
             idl0_sd_state_str(sd_state));
    if (n < 0) return;
    size_t cursor = (size_t)n;

    /* A7 (decisions, narrowed scope) / R2: card capacity + usage, directly
     * after SD: in the emit order. Omitted entirely when ABSENT (no card to
     * report on) rather than printing 0/0 — §7.3 says unknown lines are
     * ignored by the app, so adding these is additive-safe either way.
     * Values come from sd_logger's cache (idl0_sd_free_mb/idl0_sd_total_mb)
     * — never a live FATFS query here, since this runs at 1 Hz whether or
     * not a session is active (see sd_logger.c for the refresh points). */
    if (cursor < sizeof(status) && sd_state != IDL0_SD_ABSENT) {
        int add = snprintf(status + cursor, sizeof(status) - cursor,
                           "\nSD_Free: %u MB\nSD_Total: %u MB",
                           (unsigned)idl0_sd_free_mb(),
                           (unsigned)idl0_sd_total_mb());
        if (add > 0) cursor += (size_t)add;
    }

    if (cursor < sizeof(status)) {
        int add = snprintf(status + cursor, sizeof(status) - cursor,
                           "\nGPS: %s\nIMU: %s\nFirmware: %s",
                           idl0_gps_state_str(idl0_gps_state()),
                           idl0_imu_state_str(idl0_imu_state()),
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
