/* Session lifecycle: start/stop orchestration.
 *
 * The session file is the v3 header (§5.1) — schema = 3, 40-byte channel
 * registry entries — followed by IMU_SAMPLE (0x01), GPS_FIX (0x02), and
 * eventually CHANNEL_SAMPLE (0x03) records, closing with SESSION_END (0xFF).
 * session_start_utc stays 0 until the first GPS fix anchors it; the temp
 * filename carries boot_ms in the meantime (§10.2).
 */

#include "session.h"

#include <string.h>

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

#include "ble_service.h"
#include "device_id.h"
#include "digital_task.h"
#include "gps_driver.h"
#include "gps_task.h"
#include "hrm_task.h"
#include "idl0_config.h"
#include "idl0_format.h"
#include "imu_ranges.h"
#include "imu_task.h"
#include "led_status.h"
#include "mode_state.h"
#include "sd_logger.h"
#include "status.h"
#include "wifi_server.h"
#include "writer_task.h"

static const char *TAG = "session";

static bool s_running = false;
static bool s_first_fix_anchored = false;

/* Guards the s_running test-and-clear at the top of idl0_session_stop().
 * The 1 Hz status tick (esp_timer task, status.c publish_timer_cb) and a
 * BLE STOP_LOGGING command (NimBLE host task) can both observe s_running
 * true and call idl0_session_stop() back to back; without an atomic
 * test-and-clear both can pass the "already stopped?" check and interleave
 * through the drain/close/diag-log sequence below. */
static portMUX_TYPE s_running_mux = portMUX_INITIALIZER_UNLOCKED;

bool idl0_session_is_running(void) { return s_running; }

/* T03: idl0_writer_drain(500) can time out under a >500 ms SD stall — a
 * single call is not enough of a guarantee for the stop path, where a
 * missed drain silently drops trailing records or SESSION_END itself.
 * Loop until the ring buffer empties or a 5000 ms (5 s) cap is hit — 10
 * iterations of the 500 ms per-call timeout. `phase` names the call site
 * for the timeout log. */
static void idl0_session_drain_or_log(const char *phase)
{
    for (int i = 0; i < 10; i++) {          /* 10 x 500 ms = 5000 ms (5 s) cap */
        if (idl0_writer_drain(500 /* ms */)) {
            return;
        }
    }
    ESP_LOGE(TAG, "stop: writer drain timed out after 10 iterations (5000 ms) before %s",
             phase);
}

/* IMU axis descriptors: one entry per bit position in imu_channel_mask.
 * Bits 0-5 = IMU0 (accel XYZ then gyro XYZ), 6-11 = IMU1, 12-17 = IMU2.
 * Indexed by (imu_index * 6 + axis_within_imu). */
static const struct {
    const char *name;   /* null-terminated, <= 20 chars */
    const char *units;  /* null-terminated, <= 8 chars */
    bool        is_gyro;
} s_imu_axis_meta[18] = {
    { "IMU0_AccelX", "g",   false },
    { "IMU0_AccelY", "g",   false },
    { "IMU0_AccelZ", "g",   false },
    { "IMU0_GyroX",  "dps", true  },
    { "IMU0_GyroY",  "dps", true  },
    { "IMU0_GyroZ",  "dps", true  },
    { "IMU1_AccelX", "g",   false },
    { "IMU1_AccelY", "g",   false },
    { "IMU1_AccelZ", "g",   false },
    { "IMU1_GyroX",  "dps", true  },
    { "IMU1_GyroY",  "dps", true  },
    { "IMU1_GyroZ",  "dps", true  },
    { "IMU2_AccelX", "g",   false },
    { "IMU2_AccelY", "g",   false },
    { "IMU2_AccelZ", "g",   false },
    { "IMU2_GyroX",  "dps", true  },
    { "IMU2_GyroY",  "dps", true  },
    { "IMU2_GyroZ",  "dps", true  },
};

/* Maximum registry entries:
 *   18 IMU axes (channel IDs 0-17)
 *   2  HRM (channel IDs 22, 23) when enabled
 *   N  analog channels (user-added, ANALOG_MAX_CHANNELS=8, IDs 32+)
 *   M  digital channels (user-added, DIGITAL_MAX_CHANNELS=4, IDs after analog)
 *
 * Worst case 18 + 2 + 8 + 4 = 32. */
#define SESSION_REGISTRY_MAX 32

/* First channel_id assigned to user-added (analog/digital) channels. Skips
 * the §5.2-reserved range 18-31 (wheels at 18-19, HRM at 22-23, headroom). */
#define SESSION_USER_CHANNEL_ID_BASE 32

bool idl0_session_start(void)
{
    if (s_running) {
        ESP_LOGW(TAG, "start: already running");
        return true;
    }

    idl0_config_t cfg;
    idl0_config_load(&cfg);  /* defaults on failure — always usable */

    int64_t boot_ms = esp_timer_get_time() / 1000;
    if (!idl0_sd_open_session(boot_ms)) {
        ESP_LOGE(TAG, "start: could not open session file");
        return false;
    }

    uint8_t uuid[16];
    esp_fill_random(uuid, sizeof(uuid));

    /* T31: give any IMU that's been latched ERROR (SPI) / inactive-after-
     * attempt (I²C) since boot one more init attempt before this session's
     * channel registry (built next) locks in which axes actually get
     * recorded — a transient bus fault earlier in the run shouldn't leave
     * an IMU dropped out of every session for the rest of boot. */
    idl0_imu_task_reinit_failed();

    /* Build the channel registry.  One entry per enabled IMU axis (mask bit
     * set); channel_id equals the bit position (0-17).  Disabled axes get no
     * entry; registry_count reflects only the entries that are present. */
    idl0_channel_registry_entry_t registry[SESSION_REGISTRY_MAX];
    uint8_t registry_count = 0;

    for (int bit = 0; bit < 18; bit++) {
        if (!(cfg.imu_channel_mask & (1u << bit))) {
            continue;  /* axis disabled — no registry entry */
        }

        int imu_index  = bit / 6;  /* 0, 1, or 2 */
        bool is_gyro   = s_imu_axis_meta[bit].is_gyro;

        idl0_channel_registry_entry_t *e = &registry[registry_count++];
        memset(e, 0, sizeof(*e));
        e->channel_id      = (uint8_t)bit;
        e->data_type       = 4;   /* i16 */
        e->sample_rate_hz  = cfg.imu_sample_rate_hz;
        e->scale = is_gyro
            ? idl1_imu_gyro_dps_per_lsb(cfg.gyro_range_dps[imu_index])
            : idl1_imu_accel_g_per_lsb(cfg.accel_range_g[imu_index]);
        e->offset = 0.0f;
        /* name and units: zero-filled by memset; strncpy leaves the rest 0. */
        strncpy((char *)e->name,  s_imu_axis_meta[bit].name,  sizeof(e->name)  - 1);
        strncpy((char *)e->units, s_imu_axis_meta[bit].units, sizeof(e->units) - 1);
    }

    /* HRM channels (§5.2): channel 22 (HR_BPM, u8, 1 Hz) and channel
     * 23 (HR_RR, u16, event-driven). Added only when the strap is
     * configured and enabled (§8). Disabled HRM produces no registry
     * entries — and therefore no records, matching the spec. */
    if (cfg.hrm.enabled && registry_count + 2 <= SESSION_REGISTRY_MAX) {
        idl0_channel_registry_entry_t *e = &registry[registry_count++];
        memset(e, 0, sizeof(*e));
        e->channel_id     = cfg.hrm.hr_channel_id;   /* 22 */
        e->data_type      = 0;                       /* u8 */
        e->sample_rate_hz = 1;
        e->scale          = 1.0f;
        e->offset         = 0.0f;
        strncpy((char *)e->name,  "HR_BPM", sizeof(e->name)  - 1);
        strncpy((char *)e->units, "bpm",    sizeof(e->units) - 1);

        e = &registry[registry_count++];
        memset(e, 0, sizeof(*e));
        e->channel_id     = cfg.hrm.rr_channel_id;   /* 23 */
        e->data_type      = 1;                       /* u16 */
        e->sample_rate_hz = 0;                       /* event-driven */
        e->scale          = 1000.0f / 1024.0f;
        e->offset         = 0.0f;
        strncpy((char *)e->name,  "HR_RR", sizeof(e->name)  - 1);
        strncpy((char *)e->units, "ms",    sizeof(e->units) - 1);
    }

    /* User-added analog channels (config.analog.channels[], §8). Channel IDs
     * assigned from SESSION_USER_CHANNEL_ID_BASE (32) monotonically; the
     * assigned id is written back into cfg.analog_channels[i].channel_id so
     * the analog reader knows which id to use when submitting records. */
    uint8_t next_user_id = SESSION_USER_CHANNEL_ID_BASE;
    for (size_t i = 0; i < cfg.analog_channel_count; i++) {
        analog_channel_t *a = &cfg.analog_channels[i];
        if (!a->enabled) continue;
        if (registry_count >= SESSION_REGISTRY_MAX) {
            ESP_LOGW(TAG, "registry full — skipping analog '%s'", a->label);
            break;
        }
        a->channel_id = next_user_id++;

        idl0_channel_registry_entry_t *e = &registry[registry_count++];
        memset(e, 0, sizeof(*e));
        e->channel_id     = a->channel_id;
        e->data_type      = 1;                                 /* u16 */
        e->sample_rate_hz = cfg.analog_sample_rate_hz;
        e->scale          = a->scale;
        e->offset         = a->offset;
        strncpy((char *)e->name,  a->label, sizeof(e->name)  - 1);
        strncpy((char *)e->units, a->units, sizeof(e->units) - 1);
    }

    /* User-added digital channels (config.digital.channels[], §8). Spec 1
     * ships kind=marker — u8 event-driven, units 'event', value = press
     * counter. The assigned channel_id is written back so digital_task can
     * reference it. */
    for (size_t i = 0; i < cfg.digital_channel_count; i++) {
        digital_channel_cfg_t *d = &cfg.digital_channels[i];
        if (!d->enabled) continue;
        if (registry_count >= SESSION_REGISTRY_MAX) {
            ESP_LOGW(TAG, "registry full — skipping digital '%s'", d->label);
            break;
        }
        d->channel_id = next_user_id++;

        idl0_channel_registry_entry_t *e = &registry[registry_count++];
        memset(e, 0, sizeof(*e));
        e->channel_id     = d->channel_id;
        e->data_type      = 0;                                 /* u8 */
        e->sample_rate_hz = 0;                                 /* event-driven */
        e->scale          = 1.0f;
        e->offset         = 0.0f;
        strncpy((char *)e->name,  d->label, sizeof(e->name)  - 1);
        strncpy((char *)e->units, "event",  sizeof(e->units) - 1);
    }

    /* Header buffer: 48-byte fixed prefix + SESSION_REGISTRY_MAX × IDL0_REGISTRY_ENTRY_BYTES
     * per-channel entries + 4-byte end marker.  Derived from the same constants used by
     * idl0_format_header() so the buffer automatically grows if SESSION_REGISTRY_MAX is raised. */
#define SESSION_HEADER_BUF_BYTES \
    (48 + (SESSION_REGISTRY_MAX) * IDL0_REGISTRY_ENTRY_BYTES + 4)
    uint8_t header[SESSION_HEADER_BUF_BYTES];
    size_t hlen = idl0_format_header(
        header, sizeof(header),
        uuid, idl0_device_id_bytes(),
        0,                         /* session_start_utc — set on first fix */
        cfg.config_crc32,
        cfg.imu_channel_mask,
        cfg.imu_count,
        cfg.imu_sample_rate_hz,
        IDL0_GPS_ACTUAL_RATE_HZ,
        registry_count, registry);
    if (hlen == 0 || !idl0_writer_submit(header, hlen)) {
        ESP_LOGE(TAG, "start: header write failed");
        /* T44: the just-created tmp_<boot_ms>.idl0 never received usable
         * data — discard it instead of close(), which would leave a 0-byte
         * file behind on the card. */
        idl0_sd_discard_session();
        return false;
    }

    /* Fault flag + per-session diag counters must all be clear BEFORE the
     * session becomes visible (s_running=true) to the 1 Hz fault tick
     * (status.c publish_timer_cb, esp_timer task). That tick checks
     * idl0_writer_sd_fault() && idl0_session_is_running() and calls
     * idl0_session_stop() on a true fault — if it preempts this function
     * after s_running flips true but before idl0_writer_clear_sd_fault()
     * runs, a stale fault from a previous faulted session reads as "fault
     * now" and the tick stops the just-started session mid-start, stranding
     * the mode bit and LED. Doing this before digital_task_start() and
     * s_running=true also closes the ISR-attach-before-diag-reset window
     * (an earlier review finding): nothing can touch digital_task's ISR
     * state before its diag counters are zeroed. */
    idl0_imu_diag_reset();   /* zero the per-session drain stats */
    /* Zero the per-session writer-drop / append-failure counters (T05/T08/
     * T21) so the session-end summary below reflects only this session. */
    idl0_writer_diag_reset();
    idl0_writer_clear_sd_fault();  /* T04: fresh session starts fault-free */
    gps_task_diag_reset();
    hrm_task_diag_reset();
    digital_task_diag_reset();

    /* T10: start the marker-button GPIO reader (queue/task/ISR allocation)
     * BEFORE s_running flips true. Other tasks treat s_running==true as
     * "logging active" and start touching digital_task state; allocating
     * after that point races them against a still-forming task. Failure
     * semantics unchanged: digital_task_start logs and returns on error,
     * leaving markers disabled for the session — start proceeds regardless.
     * digital_task_start is idempotent and bails when count==0, so this is
     * a no-op for profiles without markers. The channel_ids were just
     * assigned above in the registry loop. */
    digital_task_start(cfg.digital_channels, cfg.digital_channel_count);

    s_running = true;
    s_first_fix_anchored = false;
    idl0_led_set(IDL0_LED_BLINK_FAST);
    ESP_LOGI(TAG, "session started (schema v3, %u registry entries)", registry_count);
    idl0_mode_set_bits(IDL0_MODE_BIT_LOGGING_ACTIVE);
    idl0_status_publish();
    return true;
}

bool idl0_session_stop(void)
{
    /* Test-and-clear under a critical section: the fault tick (esp_timer
     * task) and a BLE STOP_LOGGING (NimBLE host task) can both call this
     * concurrently. Without an atomic check-and-set here, both callers can
     * observe s_running==true, both proceed past this guard, and interleave
     * through the drain/close/diag-log sequence below (double SESSION_END
     * submit, double sd_close, etc). Only the caller that actually flips
     * s_running true->false continues; the other returns early. This also
     * folds in the "stop accepting new records BEFORE submitting
     * SESSION_END" ordering the drain below depends on — sensor tasks see
     * s_running==false as soon as either caller wins the race. */
    portENTER_CRITICAL(&s_running_mux);
    bool was_running = s_running;
    s_running = false;
    portEXIT_CRITICAL(&s_running_mux);
    if (!was_running) {
        ESP_LOGW(TAG, "stop: not running");
        return true;
    }

    digital_task_stop();
    idl0_session_drain_or_log("SESSION_END submit");

    uint8_t end[3];
    size_t n = idl0_format_session_end(end);
    idl0_writer_submit(end, n);
    idl0_session_drain_or_log("session close");
    idl0_sd_close_session();

    /* Session file is closed — safe to write the drain summary to the diag log
     * now without contending with session SD writes (§1). */
    idl0_imu_diag_flush();

    /* Session diag summary (T05/T08/T21) — stop path, not the logging path,
     * so an ESP_LOGI here is fine. Surfaces the four writer-drop / append-
     * failure counters that are silently counted (not logged) during the
     * session itself. */
    ESP_LOGI(TAG, "session diag: writer_append_fail=%lu gps_drops=%lu hrm_drops=%lu digital_drops=%lu",
             (unsigned long)idl0_writer_diag_append_failures(),
             (unsigned long)gps_task_diag_writer_drops(),
             (unsigned long)hrm_task_diag_writer_drops(),
             (unsigned long)digital_task_diag_writer_drops());

    idl0_led_set(IDL0_LED_BLINK_SLOW);
    ESP_LOGI(TAG, "session stopped");
    idl0_mode_clear_bits(IDL0_MODE_BIT_LOGGING_ACTIVE);
    idl0_status_publish();
    return true;
}

void idl0_session_on_first_fix(int64_t gps_epoch_ms, int64_t device_timestamp_us)
{
    if (!s_running || s_first_fix_anchored) {
        return;
    }
    /* The on-disk header's session_start_utc was written as 0; we leave
     * it that way (FAT append-only, no patching). The app back-fills
     * the wall-clock anchor from the first GPS_FIX record in the file
     * (§5.6). */
    (void)gps_epoch_ms;
    (void)device_timestamp_us;

    /* Rename tmp_<boot_ms>.idl0 to YYYY-MM-DD_HH-MM-SS.idl0 (§10.2). */
    int64_t utc_seconds = gps_epoch_ms / 1000;
    if (idl0_sd_rename_with_timestamp(utc_seconds)) {
        ESP_LOGI(TAG, "session file renamed on first fix (UTC %lld)",
                 (long long)utc_seconds);
    }
    s_first_fix_anchored = true;
}

uint8_t idl0_session_on_ble_command(idl0_ble_command_t cmd)
{
    switch (cmd) {
        case IDL0_CMD_START_LOGGING:
            if (idl0_wifi_state() != IDL0_WIFI_OFF) {
                ESP_LOGW(TAG, "START_LOGGING refused: WiFi is on (mutex \302\2477.2)");
                return IDL0_ACK_MUTEX_REFUSED;
            }
            idl0_session_start();
            return IDL0_ACK_OK;
        case IDL0_CMD_STOP_LOGGING:  idl0_session_stop();  return IDL0_ACK_OK;
        default:                                           return IDL0_ACK_NOT_IMPLEMENTED;
    }
}
