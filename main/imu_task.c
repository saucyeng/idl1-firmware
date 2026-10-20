/* IMU polling task — drains all enabled IMUs and writes one .idl1
 * IMU_BLOCK per IMU per drain (F12) via the shared writer. Three IMUs (§3)
 * are polled sequentially every IMU_POLL_MS. Each drain records its read
 * instant (esp_timer) beside the chip's own TIMESTAMP register, plus the
 * raw low-g/gyro samples, high-g triplets and FIFO timestamp words; the
 * idl-rs parser times every sample from those. No per-sample timestamp
 * arithmetic happens here.
 */

#include "imu_task.h"

#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "driver/spi_master.h"

#include "diag_log.h"
#include "idl0_config.h"
#include "idl1_format.h"
#include "imu_driver.h"
#include "imu_driver_i2c.h"
#include "i2c_bus.h"
#include "imu_ranges.h"
#include "mode_state.h"
#include "pins.h"
#include "session.h"
#include "writer_task.h"

#define IMU_POLL_MS            20    /* drain every 20 ms — ~10 pairs per drain
                                      * at the 480 Hz default, ~39 at 1920 Hz;
                                      * the 511-word FIFO holds ~255 pairs. */
#define IMU_DRAIN_MAX_PAIRS    256   /* a full LSM6DSV320X FIFO (511 words = 255
                                      * pairs + 1). Lets one drain fully recover
                                      * after a stall instead of leaving residual
                                      * to overflow. */
#define IMU_DIAG_LOG_PERIOD_MS 5000  /* periodic serial "drained N pairs" log */
#define IMU_DRAIN_MAX_HG       256   /* high-g triplets per drain (the burst is
                                      * capped at 256 words, so never more) */
#define IMU_DRAIN_MAX_TS       16    /* timestamp words per drain: one per 32
                                      * samples, so 8 at most in a full burst */
#define IMU_EVENT_MIN_GAP_US   1000000  /* LOGGER_EVENT rate limit, per code
                                         * and source (F12) */

static const char *TAG = "imu_task";

/* Per-IMU bus selection (IDL1). IMU0 is the onboard IMU on the shared SPI
 * bus ([imu_driver.c], CS = IDL1_PIN_CS_IMU0). IMU1/IMU2 are the front/rear
 * pods on the shared I2C bus ([imu_driver_i2c.c]) at the JP1-strapped
 * addresses from pins.h (decisions.md P7-H3). The SPI driver has no
 * chip-select for indices 1/2 and refuses them. */
typedef enum { IDL0_BUS_SPI, IDL0_BUS_I2C } idl0_imu_bus_t;
static const idl0_imu_bus_t s_imu_bus[IDL0_IMU_COUNT] = {
    IDL0_BUS_SPI,   /* IMU0 — onboard, SPI */
    IDL0_BUS_I2C,   /* IMU1 — front pod, I2C */
    IDL0_BUS_I2C,   /* IMU2 — rear pod, I2C */
};
static const uint8_t s_imu_i2c_addr[IDL0_IMU_COUNT] = {
    0,                          /* IMU0 not on I2C */
    IDL1_I2C_ADDR_IMU_FRONT,    /* IMU1 — 0x6A, JP1 open */
    IDL1_I2C_ADDR_IMU_REAR,     /* IMU2 — 0x6B, JP1 closed */
};

/* WHO_AM_I probe result for IMU i, from whichever driver owns its bus. */
static inline idl0_imu_probe_t imu_probe(uint8_t i) {
    return s_imu_bus[i] == IDL0_BUS_I2C
        ? idl0_imu_probe_result_i2c(i)
        : idl0_imu_probe_result(i);
}

static inline bool imu_is_active(uint8_t i) {
    return s_imu_bus[i] == IDL0_BUS_I2C
        ? idl0_imu_is_active_i2c(i)
        : idl0_imu_is_active(i);
}
/* Drain is bus-dispatched inline in drain_one() (idl0_imu_drain /
 * idl0_imu_drain_i2c); the per-word fifo_count/pop_sample helpers are gone. */
static inline bool imu_check_overrun(uint8_t i) {
    return s_imu_bus[i] == IDL0_BUS_I2C
        ? idl0_imu_check_overrun_i2c(i)
        : idl0_imu_check_overrun(i);
}
static inline void imu_take_pair_diag(uint8_t i, uint32_t *reuse, uint32_t *unknown) {
    if (s_imu_bus[i] == IDL0_BUS_I2C) {
        idl0_imu_take_pair_diag_i2c(i, reuse, unknown);
    } else {
        idl0_imu_take_pair_diag(i, reuse, unknown);
    }
}

static TaskHandle_t s_task         = NULL;
static uint16_t     s_odr_hz       = IDL1_IMU_DEFAULT_ODR_HZ;
static uint32_t     s_channel_mask = 0;

/* Boot-time per-IMU config, persisted so idl0_imu_task_reinit_failed() (T31)
 * and idl0_imu_task_state() (T32) can re-consult exactly what
 * idl0_imu_task_start() applied, without session.c/status.c duplicating
 * imu_task's bus map or config lookup. s_odr_hz above already holds the
 * one (shared) sample rate; these cover what varies per IMU index. Set once
 * in idl0_imu_task_start(); false/0 for any index the config didn't enable. */
static bool     s_imu_enabled[IDL0_IMU_COUNT];
static uint8_t  s_accel_range_g[IDL0_IMU_COUNT];
static uint16_t s_gyro_range_dps[IDL0_IMU_COUNT];
static bool     s_high_performance;
static idl1_imu_hg_cfg_t s_hg[IDL0_IMU_COUNT];   /* F11 */

/* Per-IMU counters for the periodic diagnostic log. Reset on each log emit. */
static uint32_t s_diag_pairs[IDL0_IMU_COUNT];
static uint32_t s_diag_overruns[IDL0_IMU_COUNT];
static uint32_t s_diag_writer_drops;   /* batches the writer rejected this window */
static int64_t  s_diag_last_log_us;

/* T41 / decisions.md A5: which IMUs were active when the current session
 * started. Snapshotted by imu_task_fn on the session's not-running ->
 * running edge (this task is the only reader/writer). An IMU that was
 * active then and has since dropped out emits one zero-filled IMU_SAMPLE per
 * poll cycle, so the gap is explicit in the file rather than silent. */
static bool s_active_at_session_start[IDL0_IMU_COUNT];

/* Session drain instrumentation. Accumulated in RAM across a logging session
 * and emitted to the on-SD diag log (idl0_debug.log) by idl0_imu_diag_flush()
 * AFTER the session file closes — never written mid-session (§1: raw capture
 * only). Attributes the per-drain cost (bus read vs total) so the field log can
 * show where the cycle goes and confirm the burst path. */
static uint32_t s_m_drains[IDL0_IMU_COUNT];
static uint64_t s_m_read_sum_us[IDL0_IMU_COUNT];   /* idl0_imu_drain* read time */
static uint32_t s_m_read_max_us[IDL0_IMU_COUNT];
static uint64_t s_m_total_sum_us[IDL0_IMU_COUNT];  /* full drain_one time */
static uint32_t s_m_total_max_us[IDL0_IMU_COUNT];
static uint32_t s_m_pairs[IDL0_IMU_COUNT];
static uint32_t s_m_overruns[IDL0_IMU_COUNT];
static uint32_t s_m_cycle_max_us;                  /* slowest full poll cycle */
static uint32_t s_m_writer_drops;

/* Drain scratch — static, shared, single-IMU-at-a-time (the round-robin is
 * serial, so there's no reentrancy); static keeps them off the task stack.
 * `s_block` holds one IMU_BLOCK: framing and header, every axis of a full
 * burst, the high-g triplets and the timestamp words. */
static idl0_imu_sample_t s_pairs[IMU_DRAIN_MAX_PAIRS];
static int16_t           s_hg_buf[IMU_DRAIN_MAX_HG][3];
static idl1_imu_ts_t     s_ts_buf[IMU_DRAIN_MAX_TS];
static idl1_ts_word_t    s_ts_words[IMU_DRAIN_MAX_TS];
static int16_t           s_packed[IMU_DRAIN_MAX_PAIRS * 6];
static uint8_t           s_block[IDL1_RECORD_FRAME_BYTES + IDL1_IMU_BLOCK_FIXED_BYTES
                                 + IMU_DRAIN_MAX_PAIRS * 12 + IMU_DRAIN_MAX_HG * 6
                                 + IMU_DRAIN_MAX_TS * IDL1_TS_WORD_BYTES];

/* Session counters for SESSION_SUMMARY (F12), beside the s_m_* diag ones. */
static uint32_t s_m_bus_errors[IDL0_IMU_COUNT];   /* IMU lost mid-session */
static bool     s_reported_lost[IDL0_IMU_COUNT];

/* Serialises IMU re-init between the session-start path (BLE task) and the
 * retries on the IMU task (idle and mid-session), so one IMU's device
 * handle is never removed and re-added by two tasks at once. Created in
 * idl0_imu_task_start(). */
static SemaphoreHandle_t s_reinit_lock;

/* LOGGER_EVENT rate limiting (F12): occurrences not yet written, and when
 * each code/source last wrote one. A code fires at most once a second per
 * source, carrying the count since its last record; a failed write (the
 * writer is full) keeps the count for the next try. */
#define IMU_EVT_CODES 6
static uint32_t s_evt_pending[IMU_EVT_CODES][IDL0_IMU_COUNT];
static int64_t  s_evt_last_us[IMU_EVT_CODES][IDL0_IMU_COUNT];

static void flush_event(uint8_t code, uint8_t source, int64_t now_us, bool force)
{
    uint32_t *pending = &s_evt_pending[code][source];
    if (*pending == 0) return;
    if (!force && now_us - s_evt_last_us[code][source] < IMU_EVENT_MIN_GAP_US) return;
    uint8_t rec[IDL1_RECORD_FRAME_BYTES + IDL1_LOGGER_EVENT_PAYLOAD];
    const size_t n = idl1_format_logger_event(rec, code, source, *pending, now_us);
    if (idl0_writer_submit(rec, n)) {
        *pending = 0;
        s_evt_last_us[code][source] = now_us;
    }
}

static void note_event(uint8_t code, uint8_t source, uint32_t count)
{
    s_evt_pending[code][source] += count;
    flush_event(code, source, esp_timer_get_time(), false);
}

static inline bool imu_read_chip_ts(uint8_t i, uint32_t *chip_ts) {
    return s_imu_bus[i] == IDL0_BUS_I2C
        ? idl0_imu_read_chip_ts_i2c(i, chip_ts)
        : idl0_imu_read_chip_ts(i, chip_ts);
}

static bool reinit_one_if_down(uint8_t i, bool *ok_out);

/* Mid-session recovery (Isaac, 2026-10-19: restarting a pod mid-recording
 * "better than having it be bricked"). An IMU lost during a session is
 * retried from this task, so the retry never races its own drain: every
 * SESSION_REINIT_PERIOD_MS for the first SESSION_REINIT_FAST_TRIES tries,
 * then every SESSION_REINIT_SLOW_MS. An I2C pod is probed first with a
 * short timeout, so a pod that is gone costs one ~0.1 ms NACK, not a full
 * init's worth of timeouts that would starve the other IMUs' FIFOs. */
#define SESSION_REINIT_PERIOD_MS  1000
#define SESSION_REINIT_SLOW_MS    5000
#define SESSION_REINIT_FAST_TRIES 10
#define SESSION_REINIT_PROBE_MS   5
static int64_t  s_session_retry_us[IDL0_IMU_COUNT];
static uint32_t s_session_tries[IDL0_IMU_COUNT];

static void session_reinit(uint8_t i)
{
    const int64_t now = esp_timer_get_time();
    const int64_t period_ms = s_session_tries[i] < SESSION_REINIT_FAST_TRIES
        ? SESSION_REINIT_PERIOD_MS : SESSION_REINIT_SLOW_MS;
    if (now - s_session_retry_us[i] < period_ms * 1000) return;
    s_session_retry_us[i] = now;
    s_session_tries[i]++;
    if (s_imu_bus[i] == IDL0_BUS_I2C &&
        i2c_master_probe(idl1_i2c_bus_get(), s_imu_i2c_addr[i], SESSION_REINIT_PROBE_MS) != ESP_OK) {
        return;
    }
    if (xSemaphoreTake(s_reinit_lock, 0) != pdTRUE) return;
    bool ok = false;
    (void)reinit_one_if_down(i, &ok);
    xSemaphoreGive(s_reinit_lock);
    if (ok) s_session_tries[i] = 0;
}

static void drain_one(uint8_t imu_index)
{
    /* T20: an IMU with no recorded axes has no registry entries; skip it. */
    const uint32_t imu_bits = (s_channel_mask >> (imu_index * 6)) & 0x3Fu;
    if (imu_bits == 0) return;
    const bool running = idl0_session_is_running();
    if (!imu_is_active(imu_index)) {
        /* T41 / A5, as a LOGGER_EVENT now (F12): say once that an IMU that
         * was live at session start stopped answering. */
        if (running && s_active_at_session_start[imu_index] && !s_reported_lost[imu_index]) {
            s_reported_lost[imu_index] = true;
            s_m_bus_errors[imu_index]++;
            note_event(IDL1_EVT_IMU_LOST, imu_index, 1);
        }
        if (running && s_reported_lost[imu_index]) session_reinit(imu_index);
        return;
    }
    if (s_reported_lost[imu_index] && running) {
        /* The re-init restarted the chip's FIFO and TIMESTAMP counter. The
         * IMU_RECOVERED event tells the parser a new clock segment starts
         * here (.idl1 spec §4.3), so it is written now, unthrottled, and
         * before any block from the restarted chip; if the writer is full,
         * this IMU waits a cycle rather than log blocks the parser would
         * time on the old clock. */
        s_evt_pending[IDL1_EVT_IMU_RECOVERED][imu_index] += 1;
        flush_event(IDL1_EVT_IMU_RECOVERED, imu_index, esp_timer_get_time(), true);
        if (s_evt_pending[IDL1_EVT_IMU_RECOVERED][imu_index] != 0) {
            s_evt_pending[IDL1_EVT_IMU_RECOVERED][imu_index] = 0;
            return;
        }
        s_reported_lost[imu_index] = false;
    }

    /* The clock pair (F12): esp_timer, then the chip's TIMESTAMP register
     * straight after, so the two describe one instant to within one
     * register read. */
    const int64_t t_read_us = esp_timer_get_time();
    uint32_t chip_ts = 0;
    (void)imu_read_chip_ts(imu_index, &chip_ts);

    const bool hg_on = s_hg[imu_index].enabled;
    idl1_imu_drain_extra_t extra = {
        .hg = hg_on ? s_hg_buf : NULL, .max_hg = hg_on ? IMU_DRAIN_MAX_HG : 0,
        .ts = s_ts_buf, .max_ts = IMU_DRAIN_MAX_TS,
    };
    /* Both buses use a single burst transaction (SPI via DMA, I²C via one
     * multi-byte read). */
    const size_t n_pairs = (s_imu_bus[imu_index] == IDL0_BUS_I2C)
        ? idl0_imu_drain_i2c(imu_index, s_pairs, IMU_DRAIN_MAX_PAIRS, &extra)
        : idl0_imu_drain(imu_index, s_pairs, IMU_DRAIN_MAX_PAIRS, &extra);

    const int64_t t_after_read = esp_timer_get_time();

    uint8_t flags = 0;
    if (imu_check_overrun(imu_index)) {
        flags |= 1;
        s_diag_overruns[imu_index]++;
        s_m_overruns[imu_index]++;
        if (running) note_event(IDL1_EVT_FIFO_OVERRUN, imu_index, 1);
    }

    s_diag_pairs[imu_index] += n_pairs;

    /* Per-drain instrumentation (RAM only; flushed to the diag log at session
     * end). Counts every drain so the read-vs-total split is visible. */
    const uint32_t read_us = (uint32_t)(t_after_read - t_read_us);
    s_m_drains[imu_index]++;
    s_m_pairs[imu_index] += n_pairs;
    s_m_read_sum_us[imu_index] += read_us;
    if (read_us > s_m_read_max_us[imu_index]) s_m_read_max_us[imu_index] = read_us;

    if (!running) goto account_total;  /* drained for hygiene */
    if (n_pairs == 0 && extra.n_hg == 0) goto account_total;

    /* Recorded axes in axis order (F12: any subset; all six is the default,
     * which goes straight in with no packing). */
    const int16_t *samples = &s_pairs[0].accel[0];
    uint8_t low_axes = 6;
    if (imu_bits != 0x3Fu) {
        low_axes = 0;
        for (int a = 0; a < 6; a++) low_axes += (imu_bits >> a) & 1u;
        size_t k = 0;
        for (size_t i = 0; i < n_pairs; i++) {
            const int16_t *axes = &s_pairs[i].accel[0];
            for (int a = 0; a < 6; a++) {
                if ((imu_bits >> a) & 1u) s_packed[k++] = axes[a];
            }
        }
        samples = s_packed;
    }
    for (size_t i = 0; i < extra.n_ts; i++) {
        s_ts_words[i] = (idl1_ts_word_t){
            .after_sample = s_ts_buf[i].after_pairs,
            .after_hg     = s_ts_buf[i].after_hg,
            .chip_ts      = s_ts_buf[i].chip_ts,
        };
    }
    const size_t len = idl1_format_imu_block(
        s_block, sizeof(s_block), imu_index, flags, t_read_us, chip_ts,
        samples, (uint16_t)n_pairs, low_axes,
        &s_hg_buf[0][0], (uint16_t)extra.n_hg, hg_on ? 3 : 0,
        s_ts_words, (uint8_t)extra.n_ts);
    if (len == 0 || !idl0_writer_submit(s_block, len)) {
        /* Ring buffer full — count it and say so in the file once the writer
         * has room (LOGGER_EVENT, rate-limited). Never ESP_LOGW per drop: a
         * per-batch log floods the UART on this task, delays the next drain
         * and starves the writer — a feedback loop that turns a brief
         * backlog into seconds of drops. */
        s_m_writer_drops++;
        s_diag_writer_drops++;
        s_evt_pending[IDL1_EVT_WRITER_DROP][imu_index]++;
    }

account_total:;
    const uint32_t total_us = (uint32_t)(esp_timer_get_time() - t_read_us);
    s_m_total_sum_us[imu_index] += total_us;
    if (total_us > s_m_total_max_us[imu_index]) s_m_total_max_us[imu_index] = total_us;
}

void idl1_imu_flush_events(bool force)
{
    const int64_t now = esp_timer_get_time();
    for (uint8_t code = 1; code < IMU_EVT_CODES; code++) {
        for (uint8_t i = 0; i < IDL0_IMU_COUNT; i++) flush_event(code, i, now, force);
    }
}

void idl1_imu_session_stats(uint32_t overruns[IDL0_IMU_COUNT],
                            uint32_t bus_errors[IDL0_IMU_COUNT],
                            uint32_t samples[IDL0_IMU_COUNT],
                            uint32_t *writer_drops, uint32_t *slowest_cycle_us)
{
    for (uint8_t i = 0; i < IDL0_IMU_COUNT; i++) {
        overruns[i]   = s_m_overruns[i];
        bus_errors[i] = s_m_bus_errors[i];
        samples[i]    = s_m_pairs[i];
    }
    *writer_drops     = s_m_writer_drops;
    *slowest_cycle_us = s_m_cycle_max_us;
}

int8_t idl1_imu_task_freq_fine(uint8_t imu_index)
{
    if (imu_index >= IDL0_IMU_COUNT) return 0;
    return s_imu_bus[imu_index] == IDL0_BUS_I2C
        ? idl0_imu_freq_fine_i2c(imu_index)
        : idl0_imu_freq_fine(imu_index);
}

/* Zero the session drain accumulators. Called by the session layer at start. */
void idl0_imu_diag_reset(void)
{
    memset(s_m_drains,        0, sizeof(s_m_drains));
    memset(s_m_read_sum_us,   0, sizeof(s_m_read_sum_us));
    memset(s_m_read_max_us,   0, sizeof(s_m_read_max_us));
    memset(s_m_total_sum_us,  0, sizeof(s_m_total_sum_us));
    memset(s_m_total_max_us,  0, sizeof(s_m_total_max_us));
    memset(s_m_pairs,         0, sizeof(s_m_pairs));
    memset(s_m_overruns,      0, sizeof(s_m_overruns));
    memset(s_m_bus_errors,    0, sizeof(s_m_bus_errors));
    memset(s_reported_lost,   0, sizeof(s_reported_lost));
    memset(s_evt_pending,     0, sizeof(s_evt_pending));
    s_m_cycle_max_us = 0;
    s_m_writer_drops = 0;
    /* Discard pair-builder diagnostics accumulated between sessions so the
     * session-end line reflects only this session. */
    for (uint8_t i = 0; i < IDL0_IMU_COUNT; i++) {
        imu_take_pair_diag(i, NULL, NULL);
    }
}

/* Emit the accumulated drain stats to the on-SD diag log. Called by the session
 * layer at stop, AFTER the session file is closed, so it never contends with
 * session SD writes (§1). One line per active IMU + one cycle/summary line. */
void idl0_imu_diag_flush(void)
{
    for (uint8_t i = 0; i < IDL0_IMU_COUNT; i++) {
        if (s_m_drains[i] == 0) continue;
        uint32_t reuse = 0, unknown = 0;
        imu_take_pair_diag(i, &reuse, &unknown);
        idl0_diag_log_event(
            "IMU%u %s odr_hz=%u drains=%lu pairs=%lu ovr=%lu reuse=%lu unk=%lu read_us=%lu/%lu tot_us=%lu/%lu",
            (unsigned)i, s_imu_bus[i] == IDL0_BUS_I2C ? "i2c" : "spi",
            (unsigned)s_odr_hz,
            (unsigned long)s_m_drains[i], (unsigned long)s_m_pairs[i],
            (unsigned long)s_m_overruns[i],
            (unsigned long)reuse, (unsigned long)unknown,
            (unsigned long)(s_m_read_sum_us[i] / s_m_drains[i]),
            (unsigned long)s_m_read_max_us[i],
            (unsigned long)(s_m_total_sum_us[i] / s_m_drains[i]),
            (unsigned long)s_m_total_max_us[i]);
    }
    idl0_diag_log_event("IMU cycle_max_us=%lu writer_drops=%lu poll_ms=%d cap=%d",
                        (unsigned long)s_m_cycle_max_us,
                        (unsigned long)s_m_writer_drops,
                        IMU_POLL_MS, IMU_DRAIN_MAX_PAIRS);
}

/* T32: whole-device IMU aggregate — see imu_task.h doc and imu_driver.c's
 * idl0_imu_state() for the rules this mirrors.
 *
 * Aggregate rules (identical to idl0_imu_state(), just walked per-bus):
 *   - "attempted" = this index is enabled in config (s_imu_enabled[i]) —
 *     imu_task only calls the matching init_one for enabled slots, so this
 *     is the cross-bus equivalent of the SPI driver's own init-attempted
 *     bookkeeping.
 *   - SPI (idl0_imu_per_state()): OK -> ok_count++; ERROR -> error++;
 *     ABSENT (unpopulated / never-initialised) counts toward neither.
 *   - I²C (idl0_imu_is_active_i2c() + idl0_imu_i2c_init_attempted()): active
 *     -> ok_count++; attempted-but-inactive -> error++. I²C has no ABSENT/
 *     ERROR split like SPI (just active + attempted), so any attempted IMU
 *     that isn't active counts as an error rather than a quiet ABSENT —
 *     that is the T32 fix: a dead I²C IMU must be visible, not invisible.
 *   - attempted == 0            -> ABSENT (nothing configured)
 *   - error > 0                 -> ERROR  (at least one configured IMU is down)
 *   - ok_count == 0             -> ABSENT (configured, but none ever came up)
 *   - ok_count < attempted      -> PARTIAL (some up, some down)
 *   - otherwise                 -> OK
 */
idl0_imu_state_t idl0_imu_task_state(void)
{
    int attempted = 0, ok_count = 0, error = 0;
    for (uint8_t i = 0; i < IDL0_IMU_COUNT; i++) {
        if (!s_imu_enabled[i]) continue;
        attempted++;
        if (s_imu_bus[i] == IDL0_BUS_I2C) {
            if (idl0_imu_is_active_i2c(i)) {
                ok_count++;
            } else if (idl0_imu_i2c_init_attempted(i)) {
                error++;
            }
        } else {
            const idl0_imu_state_t st = idl0_imu_per_state(i);
            if (st == IDL0_IMU_OK)         ok_count++;
            else if (st == IDL0_IMU_ERROR) error++;
        }
    }
    if (attempted == 0)       return IDL0_IMU_ABSENT;
    if (error > 0)            return IDL0_IMU_ERROR;
    if (ok_count == 0)        return IDL0_IMU_ABSENT;
    if (ok_count < attempted) return IDL0_IMU_PARTIAL;
    return IDL0_IMU_OK;
}

/* Re-run init for IMU `i` if it is configured, was brought up at boot and
 * is latched down now. Returns true if it was retried. Caller holds
 * s_reinit_lock. *ok_out gets the outcome. */
static bool reinit_one_if_down(uint8_t i, bool *ok_out)
{
    if (!s_imu_enabled[i]) return false;
    if (s_imu_bus[i] == IDL0_BUS_I2C) {
        if (idl0_imu_is_active_i2c(i)) return false;        /* already healthy */
        if (!idl0_imu_i2c_init_attempted(i)) return false;  /* never brought up at boot — nothing to retry */
        *ok_out = idl0_imu_init_one_i2c(
            i, s_imu_i2c_addr[i], s_odr_hz,
            s_accel_range_g[i], s_gyro_range_dps[i], s_high_performance, &s_hg[i]);
    } else {
        /* Only a live ERROR latch is worth retrying — ABSENT means the
         * chip never answered WHO_AM_I at boot (unpopulated/miswired),
         * which a re-init can't fix. */
        if (idl0_imu_per_state(i) != IDL0_IMU_ERROR) return false;
        *ok_out = idl0_imu_init_one(
            SPI2_HOST, i, s_odr_hz,
            s_accel_range_g[i], s_gyro_range_dps[i], s_high_performance, &s_hg[i]);
    }
    return true;
}

/* T31 — see imu_task.h doc. Called from idl0_session_start() (session.c),
 * before that session's channel registry is built. */
void idl0_imu_task_reinit_failed(void)
{
    if (s_reinit_lock == NULL) return;
    xSemaphoreTake(s_reinit_lock, portMAX_DELAY);
    for (uint8_t i = 0; i < IDL0_IMU_COUNT; i++) {
        bool ok = false;
        if (reinit_one_if_down(i, &ok)) {
            ESP_LOGW(TAG, "IMU%u (%s) re-init at session start: %s", (unsigned)i,
                     s_imu_bus[i] == IDL0_BUS_I2C ? "I2C" : "SPI",
                     ok ? "recovered" : "still down");
        }
    }
    xSemaphoreGive(s_reinit_lock);
}

/* Idle self-heal (2026-10-19): one failed transaction latches an IMU down,
 * and before this only a session start brought it back, so a single I2C
 * hiccup at boot left the front pod reading 0 samples until the next ride.
 * While no session is running the IMU task retries every latched-down IMU
 * once per IDLE_REINIT_PERIOD_MS. During a session session_reinit() does
 * the retrying instead, and marks the restart with IMU_RECOVERED. Runs on
 * the IMU task itself, so it never races drain_one(). Only an IMU that has
 * been up at least once this boot is retried: an unplugged pod is a normal
 * configuration and is not probed every second. */
#define IDLE_REINIT_PERIOD_MS 1000
static bool    s_was_active[IDL0_IMU_COUNT];
static bool    s_ever_up[IDL0_IMU_COUNT];
static int64_t s_last_idle_reinit_us;

static void idle_reinit(int64_t now_us)
{
    for (uint8_t i = 0; i < IDL0_IMU_COUNT; i++) {
        const bool active = imu_is_active(i);
        if (s_was_active[i] && !active) {
            ESP_LOGW(TAG, "IMU%u stopped answering; retrying every %d ms",
                     (unsigned)i, IDLE_REINIT_PERIOD_MS);
        }
        s_was_active[i] = active;
        if (active) s_ever_up[i] = true;
    }
    if (now_us - s_last_idle_reinit_us < (int64_t)IDLE_REINIT_PERIOD_MS * 1000) return;
    s_last_idle_reinit_us = now_us;
    if (xSemaphoreTake(s_reinit_lock, 0) != pdTRUE) return;
    for (uint8_t i = 0; i < IDL0_IMU_COUNT; i++) {
        bool ok = false;
        if (!s_ever_up[i]) continue;
        if (reinit_one_if_down(i, &ok) && ok) {
            ESP_LOGI(TAG, "IMU%u recovered (idle re-init)", (unsigned)i);
            s_was_active[i] = true;
        }
    }
    xSemaphoreGive(s_reinit_lock);
}

bool idl0_imu_task_is_active(uint8_t imu_index)
{
    if (imu_index >= IDL0_IMU_COUNT) return false;
    return imu_is_active(imu_index);
}

idl0_imu_probe_t idl0_imu_task_probe(uint8_t imu_index)
{
    if (imu_index >= IDL0_IMU_COUNT) {
        return (idl0_imu_probe_t){ .status = IDL0_IMU_PROBE_NOT_RUN };
    }
    return imu_probe(imu_index);
}

static void emit_diag_log(int64_t now_us)
{
    /* Show per-IMU pair counts + overruns since last log. Lets us
     * see at a glance whether each IMU's FIFO is filling (and at
     * roughly the expected rate vs ODR). */
    ESP_LOGI(TAG, "drained over last %u ms — "
                  "IMU0: %lu pairs ovr=%lu | "
                  "IMU1: %lu pairs ovr=%lu | "
                  "IMU2: %lu pairs ovr=%lu | writer_drops=%lu",
             (unsigned)IMU_DIAG_LOG_PERIOD_MS,
             (unsigned long)s_diag_pairs[0],    (unsigned long)s_diag_overruns[0],
             (unsigned long)s_diag_pairs[1],    (unsigned long)s_diag_overruns[1],
             (unsigned long)s_diag_pairs[2],    (unsigned long)s_diag_overruns[2],
             (unsigned long)s_diag_writer_drops);
    for (int i = 0; i < IDL0_IMU_COUNT; i++) {
        s_diag_pairs[i]    = 0;
        s_diag_overruns[i] = 0;
    }
    s_diag_writer_drops = 0;
    s_diag_last_log_us = now_us;
}

static void imu_task_fn(void *arg)
{
    (void)arg;

    s_diag_last_log_us = esp_timer_get_time();
    bool prev_wifi = (idl0_mode_get_bits() & IDL0_MODE_BIT_WIFI_UP) != 0;
    int64_t prev_cycle_us = 0;   /* start of the previous drain pass */
    bool prev_running = false;   /* session state at the previous cycle */

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(IMU_POLL_MS));

        /* §10.4 — while the SoftAP is up the WiFi/logging mutex guarantees no
         * session, so there is no consumer for IMU data. Stop draining: IMU0
         * shares the SPI2 bus with the SD card, so its per-cycle reads (plus
         * the IMU1/2 I²C bursts) steal bus-lock time and CPU from the /download
         * fread loop. Suspending lets the transfer have the bus. FIFOs simply
         * overflow while suspended; the first drain after WIFI_OFF discards the
         * stale backlog — exactly the existing no-session "hygiene" path. */
        const bool wifi_up = (idl0_mode_get_bits() & IDL0_MODE_BIT_WIFI_UP) != 0;
        if (wifi_up != prev_wifi) {
            ESP_LOGI(TAG, "%s IMU sampling (SoftAP %s)",
                     wifi_up ? "suspending" : "resuming",
                     wifi_up ? "up" : "down");
            prev_wifi = wifi_up;
            /* Re-anchor the diag window so the resume log isn't skewed by the
             * suspended gap. */
            if (!wifi_up) s_diag_last_log_us = esp_timer_get_time();
        }
        if (wifi_up) continue;

        /* T41: snapshot which IMUs are live as a session starts. */
        const bool running = idl0_session_is_running();
        if (running && !prev_running) {
            for (uint8_t i = 0; i < IDL0_IMU_COUNT; i++) {
                s_active_at_session_start[i] = imu_is_active(i);
            }
        }
        prev_running = running;

        const int64_t cycle_start_us = esp_timer_get_time();
        for (uint8_t i = 0; i < IDL0_IMU_COUNT; i++) {
            drain_one(i);
        }
        /* Rate-limited LOGGER_EVENTs that have waited out their gap, or a
         * writer that was full last time (F12). */
        if (running) idl1_imu_flush_events(false);

        /* Track the slowest full poll cycle (vTaskDelay + the three drains) for
         * the session diag summary — this is the FIFO's unattended window. */
        if (prev_cycle_us != 0 && idl0_session_is_running()) {
            const uint32_t cyc = (uint32_t)(cycle_start_us - prev_cycle_us);
            if (cyc > s_m_cycle_max_us) s_m_cycle_max_us = cyc;
        }
        prev_cycle_us = cycle_start_us;

        /* T07 — this task drains three IMU FIFOs every 20 ms; skip the ~150-
         * char serial diag line while a session is running (the writer task
         * is the only task that may touch the SD path during logging). The
         * same numbers reach the SD diag summary via idl0_imu_diag_flush()
         * at session stop — no information is lost. */
        const int64_t now_us = esp_timer_get_time();
        if (!running) idle_reinit(now_us);
        if (!idl0_session_is_running() &&
            now_us - s_diag_last_log_us >= (int64_t)IMU_DIAG_LOG_PERIOD_MS * 1000) {
            emit_diag_log(now_us);
        }
    }
}

bool idl0_imu_task_start(void)
{
    if (s_task != NULL) {
        return true;
    }
    idl0_config_t cfg;
    idl0_config_load(&cfg);  /* defaults on failure */

    /* Respect the config's per-IMU enable flags (from idl0_config.json). The
     * temporary all-on DIAG override was removed: with IMU1/IMU2 disabled in the
     * config they are not initialised (no I²C bring-up) and not polled — the
     * round-robin's imu_is_active() check short-circuits them. */
    ESP_LOGI(TAG, "imu_enabled = { %d, %d, %d } (from config)",
             cfg.imu_enabled[0], cfg.imu_enabled[1], cfg.imu_enabled[2]);

    s_odr_hz            = cfg.imu_sample_rate_hz;
    s_channel_mask      = cfg.imu_channel_mask;
    s_high_performance  = cfg.high_performance_mode;

    /* Initialise every IMU the config has enabled. Each call is
     * independent — a missing or unresponsive IMU does not block
     * its siblings, the aggregate state just reflects PARTIAL.
     * Bus selection per s_imu_bus[] above. */
    int up = 0;
    for (uint8_t i = 0; i < IDL0_IMU_COUNT; i++) {
        if (!cfg.imu_enabled[i]) continue;
        /* T31/T32: remember this IMU's boot config so a later re-init
         * (idl0_imu_task_reinit_failed()) or aggregate query
         * (idl0_imu_task_state()) doesn't need cfg re-loaded. */
        s_imu_enabled[i]    = true;
        s_accel_range_g[i]  = cfg.accel_range_g[i];
        s_gyro_range_dps[i] = cfg.gyro_range_dps[i];
        s_hg[i] = (idl1_imu_hg_cfg_t){
            .enabled = cfg.hg_enabled[i],
            .rate_hz = cfg.hg_sample_rate_hz,
            .range_g = cfg.hg_range_g,
        };
        bool ok;
        if (s_imu_bus[i] == IDL0_BUS_I2C) {
            ok = idl0_imu_init_one_i2c(i, s_imu_i2c_addr[i],
                                       cfg.imu_sample_rate_hz,
                                       cfg.accel_range_g[i],
                                       cfg.gyro_range_dps[i],
                                       cfg.high_performance_mode, &s_hg[i]);
        } else {
            ok = idl0_imu_init_one(SPI2_HOST, i,
                                   cfg.imu_sample_rate_hz,
                                   cfg.accel_range_g[i],
                                   cfg.gyro_range_dps[i],
                                   cfg.high_performance_mode, &s_hg[i]);
        }
        if (ok) up++;
    }
    if (up == 0) {
        ESP_LOGE(TAG, "no IMUs initialised");
        return false;
    }

    /* Priority 5 — equal to the SD writer. With the burst drain + 256-pair cap
     * the chip FIFO has ~300 ms of headroom (field logs show ovr=0), so the
     * drain does NOT need to outrank the writer; the binding constraint is now
     * the writer keeping the 64 KB ring drained. Equal priority time-slices the
     * two so neither starves (an earlier prio-6 inversion starved the writer at
     * session start → "buffer full" drops). The task sleeps IMU_POLL_MS between
     * short bursts, leaving the core to the writer and BLE host.
     *
     * 6 KB stack: the big drain/batch buffers are file-static, so the frame is
     * small; 6 KB keeps comfortable headroom. */
    s_reinit_lock = xSemaphoreCreateMutex();
    for (uint8_t i = 0; i < IDL0_IMU_COUNT; i++) s_was_active[i] = imu_is_active(i);
    BaseType_t ok = xTaskCreate(imu_task_fn, "imu", 6144, NULL, 5, &s_task);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate failed");
        s_task = NULL;
        return false;
    }
    ESP_LOGI(TAG, "IMU task up: %d/%d IMUs active, ODR %u Hz, mask 0x%05lX",
             up, IDL0_IMU_COUNT,
             (unsigned)s_odr_hz, (unsigned long)s_channel_mask);
    return true;
}
