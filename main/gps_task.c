#include "gps_task.h"

#include <string.h>

#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "gps_driver.h"
#include "idl0_format.h"
#include "mode_state.h"
#include "session.h"
#include "status.h"
#include "writer_task.h"

#define IDL0_GPS_UART        UART_NUM_1
#define IDL0_GPS_LINE_BUF    96
#define IDL0_GPS_READ_CHUNK  128

/* Lost-lock threshold: with the module emitting at 1 Hz, five seconds
 * of silence is well past any single-sentence checksum hiccup. */
#define IDL0_GPS_LOST_LOCK_US (5 * 1000 * 1000LL)

static const char *TAG = "gps_task";

static TaskHandle_t s_task         = NULL;
static int64_t      s_last_fix_us  = 0;  /* esp_timer at last valid RMC */
static bool         s_have_fix     = false;

/* T08 diag: GPS_FIX records the writer rejected (ring buffer full), counted
 * not logged — mirrors imu_task's s_diag_writer_drops: a per-drop ESP_LOGW
 * on this task's NMEA-parsing path would run during the session it reports
 * on. Plain uint32_t — single-core chip, increment-only diagnostics. */
static uint32_t s_diag_writer_drops;

/* Boot self-test counters (bringup.c): raw UART bytes read and '$' lines
 * dispatched, since boot. One add per read / per line — no logging, no
 * extra parsing on the logging path. Same single-core, increment-only
 * reasoning as s_diag_writer_drops. */
static uint32_t s_diag_rx_bytes;
static uint32_t s_diag_rx_sentences;

static void emit_fix(const idl0_gps_fix_t *fix)
{
    uint8_t buf[40];
    size_t n = idl0_format_gps_fix(buf,
                                   fix->gps_epoch_ms,
                                   fix->device_timestamp_us,
                                   fix->latitude_e7,
                                   fix->longitude_e7,
                                   fix->altitude_x10,
                                   fix->speed_x100,
                                   fix->heading_x100,
                                   fix->fix_quality,
                                   fix->satellites);
    if (!idl0_writer_submit(buf, n)) {
        s_diag_writer_drops++;
    }

    /* First-fix anchor: rename the session file. Idempotent inside the
     * session — only the first call has an effect. */
    idl0_session_on_first_fix(fix->gps_epoch_ms, fix->device_timestamp_us);
}

/* Dispatch one complete, NUL-terminated line (in `line[0..line_len-1]`) to
 * the integrator. Shared by the '\n' terminator and the T26 mid-buffer '$'
 * flush below, since both mark "this line is done." */
static void dispatch_line(char *line, size_t line_len)
{
    line[line_len] = '\0';
    if (line_len == 0 || line[0] != '$') return;
    s_diag_rx_sentences++;
    idl0_gps_fix_t fix = {0};
    if (idl0_gps_feed_line(line, &fix)) {
        if (idl0_session_is_running()) {
            emit_fix(&fix);
        }
        s_last_fix_us = fix.device_timestamp_us;
        if (!s_have_fix) {
            s_have_fix = true;
            idl0_status_publish();   /* GPS: NOFIX → FIX */
        }
    }
}

static void gps_task_fn(void *arg)
{
    (void)arg;
    char line[IDL0_GPS_LINE_BUF];
    size_t line_len = 0;
    uint8_t chunk[IDL0_GPS_READ_CHUNK];
    bool prev_wifi = (idl0_mode_get_bits() & IDL0_MODE_BIT_WIFI_UP) != 0;

    for (;;) {
        /* §10.4 — suspend NMEA parsing while the SoftAP is up (no session can
         * run under the mutex, so nothing is logged anyway). Frees the parsing
         * CPU for the WiFi/httpd path during a transfer. On resume, flush the
         * UART RX backlog that accumulated while suspended so we re-sync on a
         * fresh sentence rather than parsing stale bytes. */
        const bool wifi_up = (idl0_mode_get_bits() & IDL0_MODE_BIT_WIFI_UP) != 0;
        if (wifi_up != prev_wifi) {
            ESP_LOGI(TAG, "%s GPS parsing (SoftAP %s)",
                     wifi_up ? "suspending" : "resuming",
                     wifi_up ? "up" : "down");
            if (!wifi_up) {
                uart_flush_input(IDL0_GPS_UART);
                line_len = 0;
            }
            prev_wifi = wifi_up;
        }
        if (wifi_up) {
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }

        int n = uart_read_bytes(IDL0_GPS_UART, chunk, sizeof(chunk),
                                pdMS_TO_TICKS(100));
        if (n > 0) s_diag_rx_bytes += (uint32_t)n;
        for (int i = 0; i < n; i++) {
            uint8_t c = chunk[i];
            if (c == '\r') {
                continue;
            }
            if (c == '\n') {
                dispatch_line(line, line_len);
                line_len = 0;
                continue;
            }
            if (c == '$' && line_len > 0) {
                /* T26: a '$' arriving mid-buffer means the previous
                 * sentence's CR/LF terminator was dropped/corrupted and
                 * the two sentences are glued together. Dispatch what's
                 * buffered as a complete line first (its own checksum
                 * still validates it), then start the new sentence with
                 * this '$' — otherwise the trailing sentence is silently
                 * discarded when the '\n' finally arrives. */
                dispatch_line(line, line_len);
                line_len = 0;
            }
            if (line_len < sizeof(line) - 1) {
                line[line_len++] = (char)c;
            } else {
                /* Overflow — reset the buffer; resync on next '\n'. */
                line_len = 0;
            }
        }

        /* Lost-lock watchdog: if we've held a fix and no valid RMC has
         * arrived for IDL0_GPS_LOST_LOCK_US, drop back to NOFIX and
         * publish status. The driver's s_state will follow on the next
         * fresh fix via idl0_gps_feed_line. */
        if (s_have_fix &&
            (esp_timer_get_time() - s_last_fix_us) > IDL0_GPS_LOST_LOCK_US) {
            s_have_fix = false;
            idl0_gps_force_state(IDL0_GPS_NOFIX);
            idl0_status_publish();
            /* T08 — the state transition always happens; only the ESP_LOGW is
             * gated off the logging path (this task also runs outside a
             * session, where the log is still useful). */
            if (!idl0_session_is_running()) {
                ESP_LOGW(TAG, "GPS lock lost (no valid RMC for > %lld ms)",
                         (long long)(IDL0_GPS_LOST_LOCK_US / 1000));
            }
        }
    }
}

bool idl0_gps_task_start(void)
{
    if (s_task != NULL) {
        return true;
    }
    if (!idl0_gps_init()) {
        ESP_LOGE(TAG, "gps init failed");
        return false;
    }
    BaseType_t ok = xTaskCreate(gps_task_fn, "gps", 4096, NULL, 4, &s_task);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate failed");
        s_task = NULL;
        return false;
    }
    ESP_LOGI(TAG, "GPS task up");
    return true;
}

uint32_t gps_task_diag_writer_drops(void)
{
    return s_diag_writer_drops;
}

void gps_task_diag_reset(void)
{
    s_diag_writer_drops = 0;
}

uint32_t gps_task_diag_rx_bytes(void)
{
    return s_diag_rx_bytes;
}

uint32_t gps_task_diag_rx_sentences(void)
{
    return s_diag_rx_sentences;
}
