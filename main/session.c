/* Session lifecycle: start/stop orchestration.
 *
 * The session file is the v3 header (§5.1) — schema = 3, 40-byte channel
 * registry entries — followed by IMU_SAMPLE (0x01), GPS_FIX (0x02), and
 * eventually CHANNEL_SAMPLE (0x03) records, closing with SESSION_END (0xFF).
 * session_start_utc stays 0 until the first GPS fix anchors it; the temp
 * filename carries boot_ms in the meantime (§10.2).
 */

#include "session.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "writer_task.h"
#include "freertos/FreeRTOS.h"

#include "ble_service.h"
#include "device_id.h"
#include "digital_task.h"
#include "gps_driver.h"
#include "gps_task.h"
#include "hrm_task.h"
#include "idl0_config.h"
#include "esp_app_desc.h"
#include "idl1_format.h"
#include "imu_ranges.h"
#include "imu_task.h"
#include "led_status.h"
#include "mode_state.h"
#include "pins.h"
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

/* esp_timer_get_time() when the running session started, us. */
static int64_t s_started_us = 0;

uint32_t idl0_session_elapsed_s(void)
{
    if (!s_running) return 0;
    return (uint32_t)((esp_timer_get_time() - s_started_us) / 1000000);
}

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

/* Maximum registry entries (F12):
 *   18 low-g/gyro axes (channel ids 0-17)
 *    9 high-g axes (ids 18-26) for IMUs with high-g on
 *    2 wheel (ids 32, 33) when enabled
 *    2 HRM (ids 36, 37) when enabled
 *    8 analog + 4 digital user channels (ids 64+)
 * Worst case 18 + 9 + 2 + 2 + 8 + 4 = 43. */
#define SESSION_REGISTRY_MAX 48

/* First channel id for user-added analog/digital channels (F12: 64+). */
#define SESSION_USER_CHANNEL_ID_BASE IDL1_CH_USER_BASE

/* Registry and header buffers. Static: 48 x 52 bytes is too much for the
 * caller's stack (session start runs on the NimBLE host or button task),
 * and only one session starts at a time. */
static idl1_registry_entry_t s_registry[SESSION_REGISTRY_MAX];
static uint8_t s_header[IDL1_HEADER_FIXED_BYTES + SESSION_REGISTRY_MAX * IDL1_REGISTRY_ENTRY_BYTES + 4];

/* Copies `src` into a fixed-width name or units field, NUL-terminated. */
static void set_str(char *dst, size_t width, const char *src)
{
    strncpy(dst, src, width - 1);
    dst[width - 1] = '\0';
}

bool idl0_session_start(void)
{
    if (s_running) {
        ESP_LOGW(TAG, "start: already running");
        return true;
    }

    /* The writer's buffer is released while WiFi mode is up and restored
     * when it ends; retry here so a failed restore can't silently drop a
     * whole session. */
    if (!idl0_writer_restore()) {
        ESP_LOGE(TAG, "start: no writer buffer (out of memory)");
        return false;
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

    /* Build the channel registry (F12). One entry per recorded low-g/gyro
     * axis (channel id = mask bit, 0-17), one per high-g axis of an IMU with
     * high-g on (18-26), then HRM and the user channels. */
    uint16_t registry_count = 0;
    for (int bit = 0; bit < 18; bit++) {
        if (!(cfg.imu_channel_mask & (1u << bit))) continue;
        const int imu_index = bit / 6;
        const bool is_gyro  = s_imu_axis_meta[bit].is_gyro;
        idl1_registry_entry_t *e = &s_registry[registry_count++];
        memset(e, 0, sizeof(*e));
        e->channel_id = (uint16_t)bit;
        e->data_type  = 4;   /* i16 */
        e->kind       = IDL1_KIND_IMU_AXIS;
        e->source     = (uint8_t)imu_index;
        e->axis       = (uint8_t)(bit % 6);
        e->path       = IDL1_PATH_LOW_G;
        e->rate_mhz   = (uint32_t)cfg.imu_sample_rate_hz * 1000u;
        e->scale = is_gyro
            ? idl1_imu_gyro_dps_per_lsb(cfg.gyro_range_dps[imu_index])
            : idl1_imu_accel_g_per_lsb(cfg.accel_range_g[imu_index]);
        set_str(e->name, sizeof(e->name), s_imu_axis_meta[bit].name);
        set_str(e->units, sizeof(e->units), s_imu_axis_meta[bit].units);
    }
    static const char *const hg_axis_name[3] = { "HgAccelX", "HgAccelY", "HgAccelZ" };
    for (int imu = 0; imu < 3; imu++) {
        const uint32_t imu_bits = (cfg.imu_channel_mask >> (imu * 6)) & 0x3Fu;
        if (!cfg.hg_enabled[imu] || !cfg.imu_enabled[imu] || imu_bits == 0) continue;
        for (int axis = 0; axis < 3; axis++) {
            idl1_registry_entry_t *e = &s_registry[registry_count++];
            memset(e, 0, sizeof(*e));
            e->channel_id = (uint16_t)(IDL1_CH_HIGH_G_BASE + imu * 3 + axis);
            e->data_type  = 4;   /* i16 */
            e->kind       = IDL1_KIND_IMU_AXIS;
            e->source     = (uint8_t)imu;
            e->axis       = (uint8_t)axis;
            e->path       = IDL1_PATH_HIGH_G;
            e->rate_mhz   = (uint32_t)cfg.hg_sample_rate_hz * 1000u;
            e->scale      = idl1_imu_hg_g_per_lsb(cfg.hg_range_g);
            snprintf(e->name, sizeof(e->name), "IMU%d_%s", imu, hg_axis_name[axis]);
            set_str(e->units, sizeof(e->units), "g");
        }
    }

    /* Wheel speed (PT18, spec §5.2 / §8): WheelFront 32, WheelRear 33, u32
     * monotonic pulse count, one event-driven CHANNEL_SAMPLE per tooth.
     * Handed to digital_task alongside the markers below. */
    digital_channel_cfg_t digital[DIGITAL_TASK_MAX_CHANNELS];
    size_t digital_count = 0;
    static const struct {
        uint16_t   channel_id;
        gpio_num_t pin;
        const char *name;
    } wheel_meta[2] = {
        { IDL1_CH_WHEEL_FRONT, IDL1_PIN_WHEEL_F, "WheelFront" },
        { IDL1_CH_WHEEL_REAR,  IDL1_PIN_WHEEL_R, "WheelRear"  },
    };
    for (int w = 0; w < 2; w++) {
        if (!cfg.wheel_enabled[w]) continue;
        idl1_registry_entry_t *e = &s_registry[registry_count++];
        memset(e, 0, sizeof(*e));
        e->channel_id = wheel_meta[w].channel_id;
        e->data_type  = 2;   /* u32 */
        e->rate_mhz   = 0;   /* event-driven */
        e->scale      = 1.0f;
        set_str(e->name, sizeof(e->name), wheel_meta[w].name);
        set_str(e->units, sizeof(e->units), "pulse");

        digital_channel_cfg_t *d = &digital[digital_count++];
        memset(d, 0, sizeof(*d));
        d->enabled         = true;
        d->kind            = DIGITAL_KIND_WHEEL;
        d->gpio_pin        = wheel_meta[w].pin;
        d->min_interval_us = DIGITAL_WHEEL_MIN_INTERVAL_US;
        d->channel_id      = (uint8_t)wheel_meta[w].channel_id;
        set_str(d->label, sizeof(d->label), wheel_meta[w].name);
    }

    /* HRM channels: HR_BPM (u8, 1 Hz) and HR_RR (u16, event-driven), only
     * when the strap is configured and enabled (§8). */
    if (cfg.hrm.enabled && registry_count + 2 <= SESSION_REGISTRY_MAX) {
        idl1_registry_entry_t *e = &s_registry[registry_count++];
        memset(e, 0, sizeof(*e));
        e->channel_id = cfg.hrm.hr_channel_id;   /* 36 */
        e->data_type  = 0;                       /* u8 */
        e->rate_mhz   = 1000;
        e->scale      = 1.0f;
        set_str(e->name, sizeof(e->name), "HR_BPM");
        set_str(e->units, sizeof(e->units), "bpm");

        e = &s_registry[registry_count++];
        memset(e, 0, sizeof(*e));
        e->channel_id = cfg.hrm.rr_channel_id;   /* 37 */
        e->data_type  = 1;                       /* u16 */
        e->rate_mhz   = 0;                       /* event-driven */
        e->scale      = 1000.0f / 1024.0f;
        set_str(e->name, sizeof(e->name), "HR_RR");
        set_str(e->units, sizeof(e->units), "ms");
    }

    /* User-added analog channels (config.analog.channels[], §8): ids from
     * SESSION_USER_CHANNEL_ID_BASE (64) upward, written back so the reader
     * knows which id to submit records under. */
    uint8_t next_user_id = SESSION_USER_CHANNEL_ID_BASE;
    for (size_t i = 0; i < cfg.analog_channel_count; i++) {
        analog_channel_t *a = &cfg.analog_channels[i];
        if (!a->enabled) continue;
        if (registry_count >= SESSION_REGISTRY_MAX) {
            ESP_LOGW(TAG, "registry full — skipping analog '%s'", a->label);
            break;
        }
        a->channel_id = next_user_id++;
        idl1_registry_entry_t *e = &s_registry[registry_count++];
        memset(e, 0, sizeof(*e));
        e->channel_id = a->channel_id;
        e->data_type  = 1;                                          /* u16 */
        e->rate_mhz   = (uint32_t)cfg.analog_sample_rate_hz * 1000u;
        e->scale      = a->scale;
        e->offset     = a->offset;
        set_str(e->name, sizeof(e->name), a->label);
        set_str(e->units, sizeof(e->units), a->units);
    }

    /* User-added digital channels (config.digital.channels[], §8): markers,
     * u8 event-driven press counters. */
    for (size_t i = 0; i < cfg.digital_channel_count; i++) {
        digital_channel_cfg_t *d = &cfg.digital_channels[i];
        if (!d->enabled) continue;
        if (registry_count >= SESSION_REGISTRY_MAX) {
            ESP_LOGW(TAG, "registry full — skipping digital '%s'", d->label);
            break;
        }
        d->channel_id = next_user_id++;
        idl1_registry_entry_t *e = &s_registry[registry_count++];
        memset(e, 0, sizeof(*e));
        e->channel_id = d->channel_id;
        e->data_type  = 0;                                          /* u8 */
        e->rate_mhz   = 0;                                          /* event-driven */
        e->scale      = 1.0f;
        set_str(e->name, sizeof(e->name), d->label);
        set_str(e->units, sizeof(e->units), "event");
        digital[digital_count++] = *d;
    }

    const int8_t freq_fine[3] = {
        idl1_imu_task_freq_fine(0), idl1_imu_task_freq_fine(1), idl1_imu_task_freq_fine(2),
    };
    const size_t hlen = idl1_format_header(
        s_header, sizeof(s_header),
        uuid, idl0_device_id_bytes(),
        esp_app_get_description()->version,
        0,                         /* session_start_utc — back-filled from the first fix */
        cfg.config_crc32,
        freq_fine,
        registry_count, s_registry);
    const uint8_t *header = s_header;
    if (hlen == 0 || !idl0_writer_submit(header, hlen)) {
        ESP_LOGE(TAG, "start: header write failed");
        /* T44: the just-created tmp_<boot_ms>.idl1 never received usable
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
     * leaving markers and wheels disabled for the session — start proceeds
     * regardless. digital_task_start is idempotent and bails when count==0,
     * so this is a no-op for profiles without markers or wheels. The
     * channel_ids were just assigned above in the registry loops. */
    digital_task_start(digital, digital_count);

    s_started_us = esp_timer_get_time();
    s_running = true;
    s_first_fix_anchored = false;
    idl0_led_set(IDL0_LED_BLINK_FAST);
    ESP_LOGI(TAG, "session started (.idl1 v%d, %u registry entries)", IDL1_FORMAT_VERSION,
             (unsigned)registry_count);
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
    idl0_session_drain_or_log("SESSION_SUMMARY submit");

    /* F12: any rate-limited LOGGER_EVENTs still waiting, then the session's
     * counters, then SESSION_END. */
    idl1_imu_flush_events(true);
    uint32_t overruns[3], bus_errors[3], samples[3], writer_drops = 0, slowest = 0;
    idl1_imu_session_stats(overruns, bus_errors, samples, &writer_drops, &slowest);
    uint8_t summary[IDL1_RECORD_FRAME_BYTES + 1 + 3 * 12 + 8];
    const size_t sn = idl1_format_session_summary(summary, sizeof(summary), 3, overruns,
                                                  bus_errors, samples, writer_drops, slowest);
    if (sn > 0) idl0_writer_submit(summary, sn);

    uint8_t end[IDL1_RECORD_FRAME_BYTES];
    size_t n = idl1_format_session_end(end);
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

    /* Rename tmp_<boot_ms>.idl1 to YYYY-MM-DD_HH-MM-SS.idl1 (§10.2). */
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
