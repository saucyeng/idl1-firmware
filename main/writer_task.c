/* Single-consumer SD writer — see writer_task.h. */

#include "writer_task.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "sd_logger.h"

static const char *TAG = "writer";

#define WRITER_FLUSH_PERIOD_US (1000 * 1000)   /* durability flush cadence (~1 s) */

static RingbufHandle_t s_rb   = NULL;
static TaskHandle_t    s_task = NULL;
/* Held by the writer task around each receive/append/return, and by
 * idl0_writer_release() while it deletes the buffer — so the buffer is
 * never deleted under the writer. Producers never take it (the logging hot
 * path stays lock-free); they only run during sessions, and a session
 * never overlaps a release (WiFi mode). */
static SemaphoreHandle_t s_rb_lock = NULL;

/* T05 diag: idl0_sd_append failures, counted not logged per-item — the writer
 * is the sole consumer draining at producer batch rate (~20 ms); a per-item
 * ESP_LOGW would throttle the pipeline during the very failure it reports.
 * Plain uint32_t — single-core chip, increment-only diagnostics. */
static uint32_t s_append_fail_count;
static bool     s_append_fail_logged;  /* true once ESP_LOGW fired for this failing run */

/* T04: latched once an append failure lines up with SD FULL/ERROR. Written
 * only here (writer task); read by idl0_writer_sd_fault() from the esp_timer
 * task (status.c's 1 Hz publish callback). volatile makes that cross-task
 * read see the write promptly; no atomics needed for the bool itself — the
 * C6 is single-core, so a plain bool store/load can't tear. */
static volatile bool s_sd_fault;

static void writer_task_fn(void *arg)
{
    (void)arg;
    int64_t last_flush_us = esp_timer_get_time();
    for (;;) {
        xSemaphoreTake(s_rb_lock, portMAX_DELAY);
        RingbufHandle_t rb = s_rb;
        if (rb == NULL) {
            /* Released for WiFi mode: idle until it comes back. */
            xSemaphoreGive(s_rb_lock);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        size_t item_size = 0;
        void *item = xRingbufferReceive(rb, &item_size, pdMS_TO_TICKS(100));
        if (item != NULL) {
            if (idl0_sd_append((const uint8_t *)item, item_size)) {
                s_append_fail_logged = false;
            } else {
                s_append_fail_count++;
                if (!s_append_fail_logged) {
                    /* One ESP_LOGW per transition into the failing state, not
                     * per failed append (T05) — the running total reaches the
                     * session-end diag summary via
                     * idl0_writer_diag_append_failures(). */
                    ESP_LOGW(TAG, "append failing (%u bytes) — further failures counted silently",
                             (unsigned)item_size);
                    s_append_fail_logged = true;
                }
                /* T04: an append failure while the card reads FULL or ERROR
                 * means the session can no longer log — latch the fault so
                 * the status timer stops the session and reports it over BLE
                 * (§10.2 / A7). Any other append failure (e.g. a transient
                 * SPI error state hasn't caught up to yet) leaves the flag
                 * alone. */
                idl0_sd_state_t sd_state = idl0_sd_state();
                if (sd_state == IDL0_SD_FULL || sd_state == IDL0_SD_ERROR) {
                    s_sd_fault = true;
                }
            }
            vRingbufferReturnItem(rb, item);
        }
        xSemaphoreGive(s_rb_lock);
        /* Periodic durability flush + fsync. idl0_sd_append fills a 16 KB
         * cluster buffer without flushing (so it doesn't hold the shared SPI
         * bus on every batch); idl0_sd_flush pushes that buffer through AND
         * fsyncs the directory entry, so a power loss costs at most ~1 s of
         * samples and the on-disk file SIZE never freezes mid-session — FATFS
         * otherwise commits the size only at close (§10.2 / §5.3). No-op when
         * no session file is open. */
        const int64_t now_us = esp_timer_get_time();
        if (now_us - last_flush_us >= WRITER_FLUSH_PERIOD_US) {
            idl0_sd_flush();
            last_flush_us = now_us;
        }
    }
}

bool idl0_writer_init(void)
{
    if (s_task != NULL) {
        return true;
    }
    s_rb_lock = xSemaphoreCreateMutex();
    if (s_rb_lock == NULL) {
        ESP_LOGE(TAG, "lock create failed");
        return false;
    }
    s_rb = xRingbufferCreate(IDL0_WRITER_BUFFER_BYTES, RINGBUF_TYPE_NOSPLIT);
    if (s_rb == NULL) {
        ESP_LOGE(TAG, "ring buffer alloc failed");
        return false;
    }
    BaseType_t ok = xTaskCreate(writer_task_fn, "writer", 4096, NULL, 5, &s_task);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate failed");
        return false;
    }
    ESP_LOGI(TAG, "writer task up (%u-byte buffer)", IDL0_WRITER_BUFFER_BYTES);
    return true;
}

void idl0_writer_release(void)
{
    if (s_rb_lock == NULL) {
        return;
    }
    xSemaphoreTake(s_rb_lock, portMAX_DELAY);
    if (s_rb != NULL) {
        RingbufHandle_t rb = s_rb;
        s_rb = NULL;
        vRingbufferDelete(rb);
        ESP_LOGI(TAG, "ring buffer released (WiFi mode)");
    }
    xSemaphoreGive(s_rb_lock);
}

bool idl0_writer_restore(void)
{
    if (s_rb_lock == NULL) {
        return false;
    }
    xSemaphoreTake(s_rb_lock, portMAX_DELAY);
    if (s_rb == NULL) {
        s_rb = xRingbufferCreate(IDL0_WRITER_BUFFER_BYTES, RINGBUF_TYPE_NOSPLIT);
        if (s_rb == NULL) {
            ESP_LOGE(TAG, "ring buffer re-create failed");
        } else {
            ESP_LOGI(TAG, "ring buffer restored");
        }
    }
    bool ok = s_rb != NULL;
    xSemaphoreGive(s_rb_lock);
    return ok;
}

bool idl0_writer_submit(const uint8_t *buf, size_t len)
{
    if (s_rb == NULL) {
        return false;
    }
    /* Drop on full — never block the producer. SESSION_END uses a longer
     * timeout via _drain (caller's responsibility). No log here: the IMU task
     * accounts for the drop and reports a count in its 5 s diag line — a
     * per-drop ESP_LOGW floods the console and feeds back into more drops. */
    BaseType_t sent = xRingbufferSend(s_rb, buf, len, 0);
    if (sent != pdTRUE) {
        return false;
    }
    return true;
}

bool idl0_writer_drain(uint32_t timeout_ms)
{
    if (s_rb == NULL) {
        return true;
    }
    uint32_t elapsed = 0;
    while (elapsed < timeout_ms) {
        UBaseType_t items = 0;
        vRingbufferGetInfo(s_rb, NULL, NULL, NULL, NULL, &items);
        if (items == 0) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        elapsed += 10;
    }
    ESP_LOGW(TAG, "drain timeout (%u ms) — items still queued", (unsigned)timeout_ms);
    return false;
}

uint32_t idl0_writer_diag_append_failures(void)
{
    return s_append_fail_count;
}

void idl0_writer_diag_reset(void)
{
    s_append_fail_count  = 0;
    s_append_fail_logged = false;
}

bool idl0_writer_sd_fault(void)
{
    return s_sd_fault;
}

void idl0_writer_clear_sd_fault(void)
{
    s_sd_fault = false;
}
