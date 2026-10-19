/* See wifi_server.h. P8 M1: AP bring-up + state only. M2-M5 add the
 * four HTTP endpoints. */

#include "wifi_server.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "battery.h"
#include "ble_service.h"
#include "device_id.h"
#include "diag_log.h"
#include "status.h"

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>

#include "cJSON.h"

#include "idl0_config.h"
#include "mode_state.h"
#include "ota.h"
#include "ota_guard.h"
#include "sd_logger.h"
#include "session_files.h"
#include "writer_task.h"
#include "esp_heap_caps.h"


static const char *TAG = "wifi";

/* One line of internal-heap numbers (bytes) at a WiFi milestone: free, the
 * largest block, the low-water mark, and DMA-capable free (SD reads need a
 * DMA block). WiFi mode only, never on the logging path. */
static void log_heap(const char *where)
{
    ESP_LOGI(TAG, "heap @%s: free=%u largest=%u min=%u dma=%u", where,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA));
}

#define IDL0_WIFI_PASSWORD       "datalogger123"
#define IDL0_WIFI_CHANNEL        1
#define IDL0_WIFI_MAX_CONN       4

static idl0_wifi_state_t s_state    = IDL0_WIFI_OFF;
static esp_netif_t      *s_ap_netif = NULL;
static httpd_handle_t    s_httpd    = NULL;
static bool              s_event_loop_inited = false;
static bool              s_wifi_inited       = false;

/* F7: set when WiFi mode is ending so a /download or /ota still streaming on
 * the httpd worker returns at its next chunk (8 KB) instead of running to the
 * end of the file. httpd_stop() joins that worker, so without this a button
 * press mid-download would wait for the whole transfer. Cleared in
 * idl0_wifi_start(). */
static volatile bool     s_abort_transfers   = false;

/* For the LEDs (idl0_wifi_activity): when /download or /ota last moved
 * bytes (esp_timer µs), and whether an OTA image is being checked. */
static volatile int64_t  s_last_dl_us        = 0;
static volatile int64_t  s_last_ota_us       = 0;
static volatile bool     s_ota_verifying     = false;

/* Serialises idl0_wifi_stop() across its callers: the esp_timer task
 * (POST /wifi_off, idl0_wifi_stop_deferred), the failsafe task and, since
 * F7, the button task. Without it two callers could both httpd_stop() the
 * same handle. Created in idl0_wifi_start(); NULL means WiFi never started,
 * so there is nothing to stop. */
static SemaphoreHandle_t s_stop_lock         = NULL;

/* Last HTTP request time, µs since boot — one input to the §10.4
 * no-activity failsafe. Touched by every endpoint handler; initialised
 * in idl0_wifi_start so the window starts at AP-up. */
static int64_t s_last_http_us = 0;

static void touch_http_activity(void)
{
    s_last_http_us = esp_timer_get_time();
}

idl0_wifi_state_t idl0_wifi_state(void) { return s_state; }

const char *idl0_wifi_state_str(idl0_wifi_state_t state)
{
    switch (state) {
        case IDL0_WIFI_ON:  return "ON";
        case IDL0_WIFI_OFF:
        default:            return "OFF";
    }
}

/* Reads ?file=<name> from the query string into `out` (NUL-terminated).
 * Returns true on success. Rejects empty names and names containing
 * '/' or '\' (path-traversal guard — only direct children of
 * /sessions/ are reachable). */
static bool read_file_param(httpd_req_t *req, char *out, size_t out_len)
{
    size_t qlen = httpd_req_get_url_query_len(req) + 1;
    if (qlen <= 1 || qlen > 256) return false;
    char query[256];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        return false;
    }
    if (httpd_query_key_value(query, "file", out, out_len) != ESP_OK) {
        return false;
    }
    if (out[0] == '\0') return false;
    for (const char *p = out; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\') return false;
    }
    return true;
}

/* Appends `src` to `dst` as a JSON string body (no quotes), escaping `"`,
 * `\` and control characters; truncates rather than overflow. */
static void json_escape(char *dst, size_t cap, const char *src)
{
    size_t o = 0;
    for (const char *p = src; *p != '\0' && o + 7 < cap; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') {
            dst[o++] = '\\';
            dst[o++] = (char)c;
        } else if (c < 0x20) {
            o += (size_t)snprintf(dst + o, cap - o, "\\u%04x", c);
        } else {
            dst[o++] = (char)c;
        }
    }
    dst[o] = '\0';
}

/* GET /files (§6.1). Streamed one entry per chunk: building the whole array
 * in memory (cJSON) failed with a 500 once the card held many sessions and
 * WiFi plus BLE had taken the heap (2026-10-16), so nothing here grows with
 * the number of files. */
static esp_err_t files_get_handler(httpd_req_t *req)
{
    touch_http_activity();
    ESP_LOGI(TAG, "GET /files: enumerating");

    httpd_resp_set_type(req, "application/json");
    if (httpd_resp_send_chunk(req, "[", 1) != ESP_OK) return ESP_FAIL;

    int file_count = 0;
    DIR *dir = opendir(IDL0_SESSIONS_DIR);
    if (dir != NULL) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_name[0] == '.') continue;  /* skip "." / ".." */
            char path[96];
            int n = snprintf(path, sizeof(path), "%s/%s",
                             IDL0_SESSIONS_DIR, ent->d_name);
            if (n < 0 || (size_t)n >= sizeof(path)) continue;
            struct stat st;
            if (stat(path, &st) != 0) continue;

            char name[160];
            json_escape(name, sizeof(name), ent->d_name);
            /* session_id lets the app diff device files against its library
             * without downloading them (§6.1). Omitted if the header can't
             * be read; the app then shows the file as identity-unknown. */
            char sid[33];
            char sid_field[64] = "";
            if (idl0_read_session_id_hex(path, sid)) {
                snprintf(sid_field, sizeof(sid_field), ",\"session_id\":\"%s\"", sid);
            }
            char entry[288];
            int len = snprintf(entry, sizeof(entry), "%s{\"name\":\"%s\",\"size\":%ld%s}",
                               file_count == 0 ? "" : ",", name, (long)st.st_size, sid_field);
            if (len < 0 || (size_t)len >= sizeof(entry)) continue;
            if (httpd_resp_send_chunk(req, entry, len) != ESP_OK) {
                closedir(dir);
                return ESP_FAIL;
            }
            file_count++;
        }
        closedir(dir);
    }

    if (httpd_resp_send_chunk(req, "]", 1) != ESP_OK) return ESP_FAIL;
    ESP_LOGI(TAG, "GET /files: sent %d entries", file_count);
    return httpd_resp_send_chunk(req, NULL, 0);  /* end of chunked stream */
}

/* 8 KB amortises chunk framing + fread + TCP send overhead. With the
 * tuned lwIP send buffer + window from Task 1b, /download hits the
 * 500 KB/s+ band the C6 AP is capable of. */
#define IDL0_DOWNLOAD_CHUNK 8192

static esp_err_t download_get_handler(httpd_req_t *req)
{
    touch_http_activity();
    char fname[64];
    if (!read_file_param(req, fname, sizeof(fname))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing or invalid 'file'");
        return ESP_FAIL;
    }

    char path[96];
    int n = snprintf(path, sizeof(path), "%s/%s", IDL0_SESSIONS_DIR, fname);
    if (n < 0 || (size_t)n >= sizeof(path)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "name too long");
        return ESP_FAIL;
    }

    struct stat st;
    if (stat(path, &st) != 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "no such file");
        return ESP_FAIL;
    }
    long total = (long)st.st_size;
    long start = 0, end = total - 1;
    bool partial = false;

    /* Range header: "bytes=START-END", "bytes=START-", or the suffix form
     * "bytes=-N" meaning "the last N bytes" (RFC 7233 §2.1). */
    char range[64];
    if (httpd_req_get_hdr_value_str(req, "Range", range, sizeof(range)) == ESP_OK) {
        static const char prefix[] = "bytes=";
        size_t prefix_len = sizeof(prefix) - 1;
        if (strncmp(range, prefix, prefix_len) == 0 && range[prefix_len] == '-') {
            /* Suffix form: sscanf("bytes=%ld-%ld", ...) below would parse
             * this as rs=-N, which fails the rs >= 0 guard and drops the
             * whole header (client got 200 + full file instead of 206 +
             * tail) — detect it explicitly instead. */
            long n = 0;
            if (sscanf(range + prefix_len + 1, "%ld", &n) == 1 && n > 0) {
                start   = (n < total) ? total - n : 0;
                end     = total - 1;
                partial = true;
            }
            /* n <= 0 is malformed — ignore the header, same as today. */
        } else {
            long rs = 0, re = -1;
            if (sscanf(range, "bytes=%ld-%ld", &rs, &re) >= 1) {
                if (rs >= 0 && rs < total) {
                    start   = rs;
                    end     = (re >= 0 && re < total) ? re : total - 1;
                    partial = true;
                }
            }
        }
    }

    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        /* Name the cause in the body (the app shows it): "Too many open
         * files" means max_files is exhausted by a leaked handle. */
        int err = errno;
        char msg[96];
        snprintf(msg, sizeof(msg), "fopen failed: %s (errno %d)", strerror(err), err);
        ESP_LOGE(TAG, "/download %s: %s", fname, msg);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, msg);
        return ESP_FAIL;
    }
    if (start > 0) {
        if (fseek(f, start, SEEK_SET) != 0) {
            fclose(f);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "seek failed");
            return ESP_FAIL;
        }
    }

    httpd_resp_set_type(req, "application/octet-stream");
    /* Streamed via httpd_resp_send_chunk → Transfer-Encoding: chunked.
     * We deliberately do NOT set Content-Length (mixing it with chunked
     * is undefined behaviour). For partial responses, Content-Range is
     * still meaningful and the client (Dart `http.StreamedResponse`)
     * accepts a null content length. */
    if (partial) {
        char crange[64];
        snprintf(crange, sizeof(crange), "bytes %ld-%ld/%ld", start, end, total);
        httpd_resp_set_hdr(req, "Content-Range", crange);
        httpd_resp_set_status(req, "206 Partial Content");
    }

    uint8_t buf[IDL0_DOWNLOAD_CHUNK];
    long remaining = end - start + 1;
    while (remaining > 0) {
        if (s_abort_transfers) {
            /* F7: WiFi mode is ending (record button). The client sees a
             * truncated stream and resumes later with Range (§6.1). */
            ESP_LOGW(TAG, "/download aborted: WiFi mode ending");
            fclose(f);
            return ESP_FAIL;
        }
        size_t want = (remaining > (long)sizeof(buf)) ? sizeof(buf) : (size_t)remaining;
        size_t got = fread(buf, 1, want, f);
        if (got == 0) {
            /* A read error (an SD read that could not get memory, say) must
             * not end the chunked stream as if the file were complete: drop
             * the connection so the client sees a short transfer and
             * resumes (§6.1). */
            ESP_LOGE(TAG, "/download %s: read failed with %ld bytes left", fname, remaining);
            log_heap("download read failed");
            fclose(f);
            return ESP_FAIL;
        }
        if (httpd_resp_send_chunk(req, (const char *)buf, got) != ESP_OK) {
            fclose(f);
            return ESP_FAIL;
        }
        s_last_dl_us = esp_timer_get_time();
        /* A multi-minute download is activity: without this, a transfer
         * longer than the §10.4 idle window would trip the no-HTTP
         * failsafe arm mid-stream (handlers only touch at request start). */
        touch_http_activity();
        remaining -= got;
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);  /* end of chunked stream */
    return ESP_OK;
}

static esp_err_t delete_get_handler(httpd_req_t *req)
{
    touch_http_activity();
    char fname[64];
    if (!read_file_param(req, fname, sizeof(fname))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing or invalid 'file'");
        return ESP_FAIL;
    }
    char path[96];
    int n = snprintf(path, sizeof(path), "%s/%s", IDL0_SESSIONS_DIR, fname);
    if (n < 0 || (size_t)n >= sizeof(path)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "name too long");
        return ESP_FAIL;
    }
    if (remove(path) != 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "remove failed");
        return ESP_FAIL;
    }
    httpd_resp_sendstr(req, "deleted");
    return ESP_OK;
}

#define IDL0_CONFIG_MAX_BYTES 8192

static esp_err_t config_post_handler(httpd_req_t *req)
{
    touch_http_activity();
    int total = req->content_len;
    if (total <= 0 || total > IDL0_CONFIG_MAX_BYTES) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad content length");
        return ESP_FAIL;
    }

    char *body = malloc((size_t)total + 1);
    if (body == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
        return ESP_FAIL;
    }
    int received = 0;
    while (received < total) {
        int n = httpd_req_recv(req, body + received, total - received);
        if (n <= 0) {
            free(body);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv failed");
            return ESP_FAIL;
        }
        received += n;
    }
    body[total] = '\0';

    /* Validate + atomic-write is shared with the BLE config-push path
     * (FF05 + CMD_CONFIG_COMMIT, §7.2) — see idl0_config_write_json. */
    idl0_config_write_result_t wr = idl0_config_write_json(body, (size_t)total);
    free(body);
    switch (wr) {
        case IDL0_CONFIG_WRITE_BAD_JSON:
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "malformed JSON");
            return ESP_FAIL;
        case IDL0_CONFIG_WRITE_IO_ERROR:
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "write failed");
            return ESP_FAIL;
        case IDL0_CONFIG_WRITE_OK:
            break;
    }
    httpd_resp_sendstr(req, "ok");

    /* idl0_config.json is read at boot only (HRM enable/address set in
     * hrm_task_start, IMU ODR/ranges in idl0_imu_init_one), so reboot to apply
     * the new config in full — no chasing which subsystem hot-reloads. On
     * IDL1 the GPS sits on the PWR_EN-switched rail (pins.h); the reset
     * releases GPIO2 until app_main drives it high again, so expect the rail
     * (and the u-blox fix) to drop across the reset — unverified on hardware
     * as of bring-up phase 1. The 500 ms delay lets the 200 "ok" flush so
     * the app confirms the push before the link drops; the app then
     * re-establishes BLE and lands back in idle mode. The device boots into
     * idle (WiFi off, no session) by default. */
    ESP_LOGI(TAG, "/config saved — rebooting to apply");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;  /* unreachable */
}

/* 8 KB chunk amortises recv + esp_ota_write overhead. Same size as
 * /download — fits comfortably in the 16 KB httpd task stack. */
#define IDL0_OTA_CHUNK 8192

/* T06: recv_wait_timeout is 30 s (hcfg below). A client that stalls
 * without sending FIN/RST would otherwise have its HTTPD_SOCK_ERR_TIMEOUT
 * retried forever, wedging the single httpd worker so no other endpoint
 * can be served and httpd_stop() can never join it. Abort after this many
 * *consecutive* timeouts — 3 * 30 s = 90 s stalled — rather than on the
 * first one, since a single slow-but-alive chunk is normal under load. */
#define IDL0_OTA_MAX_CONSEC_TIMEOUTS 3

/* ota_guard.c is ESP-IDF-free for host testing, so it restates the image
 * layout; pin it to the real structs here. */
_Static_assert(sizeof(esp_image_header_t) == IDL1_OTA_IMAGE_HDR_BYTES,
               "esp_image_header_t size");
_Static_assert(sizeof(esp_image_segment_header_t) == IDL1_OTA_SEGMENT_HDR_BYTES,
               "esp_image_segment_header_t size");
_Static_assert(sizeof(esp_app_desc_t) == IDL1_OTA_APP_DESC_BYTES,
               "esp_app_desc_t size");
_Static_assert(IDL1_OTA_APP_DESC_OFFSET + offsetof(esp_app_desc_t, project_name) ==
               IDL1_OTA_PROJECT_NAME_OFFSET, "project_name offset");
_Static_assert(sizeof(((esp_app_desc_t *)0)->project_name) == IDL1_OTA_PROJECT_NAME_BYTES,
               "project_name size");
_Static_assert(ESP_IMAGE_HEADER_MAGIC == IDL1_OTA_IMAGE_MAGIC, "image magic");
_Static_assert(ESP_APP_DESC_MAGIC_WORD == IDL1_OTA_APP_DESC_MAGIC, "app desc magic");

/* F5 board guard: decide on the buffered image header whether this upload
 * may touch flash at all. Returns true to proceed; on false the HTTP error
 * has been sent and nothing has been written or erased. */
static bool ota_guard_accept(httpd_req_t *req, const uint8_t *hdr, size_t len)
{
    const char *mine = esp_app_get_description()->project_name;
    char theirs[IDL1_OTA_PROJECT_NAME_BYTES + 1];
    switch (idl1_ota_guard_check(hdr, len, mine, theirs, sizeof(theirs))) {
        case IDL1_OTA_GUARD_OK:
            return true;
        case IDL1_OTA_GUARD_WRONG_PROJECT: {
            char msg[128];
            snprintf(msg, sizeof(msg), "image is for project %s, this board runs %s",
                     theirs[0] ? theirs : "(empty)", mine);
            ESP_LOGE(TAG, "/ota rejected before any flash write: %s", msg);
            idl0_diag_log_event("ota rejected: project %s", theirs);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, msg);
            return false;
        }
        case IDL1_OTA_GUARD_BAD_IMAGE:
            ESP_LOGE(TAG, "/ota rejected: not an ESP32 app image (magic mismatch)");
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                "not an ESP32 app image (bad image header)");
            return false;
        case IDL1_OTA_GUARD_NEED_MORE:
        default:
            ESP_LOGE(TAG, "/ota rejected: upload shorter than the image header "
                          "(%u of %u bytes)", (unsigned)len,
                     (unsigned)IDL1_OTA_GUARD_HDR_BYTES);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                "image too short for an app header");
            return false;
    }
}

static esp_err_t ota_post_handler(httpd_req_t *req)
{
    touch_http_activity();

    /* size_t, not int (T35): an announced Content-Length >= 2^31 wrapped
     * negative here, which silently disabled both the partition-size
     * precheck below and the short-upload guard further down (a negative
     * `total` fails every `total > 0` check they depend on). 0 means
     * "unknown/absent" (chunked transfer, or no Content-Length header),
     * matching the existing total>0 checks. */
    size_t total = (req->content_len > 0) ? (size_t)req->content_len : 0;
    ESP_LOGI(TAG, "/ota begin, announced size=%lu bytes", (unsigned long)total);

    if (total > 0) {
        const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
        /* next==NULL: no OTA slot available at all — let idl0_ota_begin's
         * own NULL check below report that instead of duplicating it. */
        if (next != NULL && total > next->size) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "image too large");
            return ESP_FAIL;
        }
    }

    char *buf = malloc(IDL0_OTA_CHUNK);
    if (buf == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "alloc failed");
        return ESP_FAIL;
    }

    /* F5: nothing reaches flash — not even the partition erase inside
     * idl0_ota_begin() — until the image header has been buffered and the
     * board guard has accepted it. `s` stays NULL until then; the header
     * may arrive split across any number of recv()s. */
    uint8_t hdr[IDL1_OTA_GUARD_HDR_BYTES];
    size_t hdr_len = 0;
    idl0_ota_session_t *s = NULL;

    int consec_timeouts = 0;
    size_t received = 0;
    while (total == 0 || received < total) {
        if (s_abort_transfers) {
            /* F7: WiFi mode is ending (record button). Safe: the new image
             * becomes the boot partition only after the whole upload is
             * written and verified, so aborting leaves the running firmware
             * in place. */
            ESP_LOGW(TAG, "/ota aborted: WiFi mode ending");
            free(buf);
            idl0_ota_abort(s);  /* NULL-safe */
            return ESP_FAIL;
        }
        int n = httpd_req_recv(req, buf, IDL0_OTA_CHUNK);
        if (n == 0) break;                          /* EOF (chunked) */
        if (n == HTTPD_SOCK_ERR_TIMEOUT) {
            if (++consec_timeouts >= IDL0_OTA_MAX_CONSEC_TIMEOUTS) {
                free(buf);
                idl0_ota_abort(s);  /* releases the OTA handle (NULL-safe) */
                httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT,
                                    "upload stalled");
                return ESP_FAIL;
            }
            continue;  /* benign retry */
        }
        if (n < 0) {
            free(buf);
            idl0_ota_abort(s);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv failed");
            return ESP_FAIL;
        }
        consec_timeouts = 0;  /* any successful recv resets the streak */
        received += (size_t)n;
        s_last_ota_us = esp_timer_get_time();

        size_t off = 0;
        if (s == NULL) {
            size_t take = sizeof(hdr) - hdr_len;
            if (take > (size_t)n) take = (size_t)n;
            memcpy(hdr + hdr_len, buf, take);
            hdr_len += take;
            off = take;
            if (hdr_len < sizeof(hdr)) {
                touch_http_activity();
                continue;  /* header still incomplete */
            }
            if (!ota_guard_accept(req, hdr, hdr_len)) {
                free(buf);
                return ESP_FAIL;
            }
            s = idl0_ota_begin(total);
            if (s == NULL) {
                free(buf);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ota_begin failed");
                return ESP_FAIL;
            }
            if (!idl0_ota_write(s, hdr, hdr_len)) {
                free(buf);
                idl0_ota_abort(s);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "flash write failed");
                return ESP_FAIL;
            }
        }
        if ((size_t)n > off && !idl0_ota_write(s, buf + off, (size_t)n - off)) {
            free(buf);
            idl0_ota_abort(s);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "flash write failed");
            return ESP_FAIL;
        }
        /* A slow OTA upload is activity — same rationale as /download:
         * don't let the no-HTTP failsafe arm fire mid-stream. */
        touch_http_activity();
    }
    free(buf);

    /* Stream ended before the guard could decide: nothing was written. */
    if (s == NULL) {
        ota_guard_accept(req, hdr, hdr_len);   /* sends the "too short" 400 */
        return ESP_FAIL;
    }

    /* If Content-Length was set, the upload must have delivered exactly
     * that many bytes. A short stream means the client closed the
     * socket mid-upload — reject so we don't validate a truncated image
     * and commit it as boot. */
    if (total > 0 && received != total) {
        idl0_ota_abort(s);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "short upload");
        return ESP_FAIL;
    }

    s_ota_verifying = true;
    if (!idl0_ota_end(s)) {
        s_ota_verifying = false;
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "image validation failed");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "ok\n");
    ESP_LOGI(TAG, "/ota success — rebooting in 500 ms");
    /* Defer the reboot long enough for the 200 response to flush. */
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;  /* unreachable */
}

idl0_wifi_activity_t idl0_wifi_activity(void)
{
    if (s_ota_verifying) return IDL0_WIFI_ACT_OTA_VERIFY;
    if (s_state != IDL0_WIFI_ON) return IDL0_WIFI_ACT_NONE;
    const int64_t now = esp_timer_get_time();
    if (s_last_ota_us != 0 && now - s_last_ota_us < 1000000) return IDL0_WIFI_ACT_OTA_RX;
    if (s_last_dl_us != 0 && now - s_last_dl_us < 300000) return IDL0_WIFI_ACT_TRANSFER;
    return IDL0_WIFI_ACT_NONE;
}

/* GET /ping — identity + status (SPEC §6.1). The WiFi-mode status feed
 * and the app's identity/liveness probe. Allocation-light, never touches
 * the SD card: idl0_sd_state() is a cached-state accessor (same source
 * the 1 Hz BLE status publisher uses). */
static esp_err_t ping_get_handler(httpd_req_t *req)
{
    touch_http_activity();

    EventBits_t bits = idl0_mode_get_bits();
    const char *mode = (bits & IDL0_MODE_BIT_LOGGING_ACTIVE) ? "recording"
                     : (bits & IDL0_MODE_BIT_WIFI_UP)        ? "wifi"
                                                             : "idle";

    cJSON *obj = cJSON_CreateObject();
    if (obj == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    cJSON_AddStringToObject(obj, "device", idl0_device_name());
    cJSON_AddStringToObject(obj, "fw", esp_app_get_description()->version);
    cJSON_AddNumberToObject(obj, "proto", IDL0_WIFI_PROTO_VERSION);
    /* Cell voltage, mV, as the status line's BatteryMv (§7.3), plus the
     * deprecated "battery" percent that older apps require, estimated from
     * it (0 when the read fails) instead of a hard-coded 100. */
    int bat_raw = 0, bat_pin_mv = 0, bat_mv = 0;
    const char *bat_why = NULL;
    if (idl1_battery_read(&bat_raw, &bat_pin_mv, &bat_mv, &bat_why)) {
        cJSON_AddNumberToObject(obj, "battery", idl1_battery_pct_estimate(bat_mv));
        cJSON_AddNumberToObject(obj, "battery_mv", bat_mv);
    } else {
        cJSON_AddNumberToObject(obj, "battery", 0);
    }
    cJSON_AddStringToObject(obj, "sd", idl0_sd_state_str(idl0_sd_state()));
    cJSON_AddStringToObject(obj, "mode", mode);
    cJSON_AddStringToObject(obj, "ble", idl0_ble_is_suspended() ? "off" : "on");

    char *body = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    if (body == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
    free(body);
    return err;
}

/* POST /handoff — the app confirms its HTTP link is verified; drop BLE
 * so WiFi owns the radio outright (SPEC §10.4). Respond before
 * suspending: the 200 is the contract, and the suspend has no bearing
 * on it. Idempotent — a second call finds BLE already suspended. */
/* Runs the BLE suspend off the httpd task. The httpd server has a single
 * task: when the suspend ran inline here and the NimBLE call blocked,
 * every later request (the download, the OTA body, POST /wifi_off) waited
 * forever, and the failsafe's httpd_stop() then joined that blocked task,
 * so the logger stayed in WiFi mode with BLE off (2026-10-16 field runs).
 * A throwaway task means a slow BLE stack can only delay the suspend. */
static void ble_suspend_task(void *arg)
{
    (void)arg;
    /* Let the "ok" reach the phone before the radio changes hands. */
    vTaskDelay(pdMS_TO_TICKS(50));
    /* Suspend only while WiFi mode is still up, under the stop lock: a
     * starved task (an OTA write holds the CPU) that ran after
     * idl0_wifi_stop() had already resumed BLE turned Bluetooth off for
     * good, with WiFi off — the logger stopped advertising until reboot
     * (2026-10-19). Under the lock, a stop that follows will resume. */
    xSemaphoreTake(s_stop_lock, portMAX_DELAY);
    if (s_state == IDL0_WIFI_ON) {
        idl0_ble_suspend();
        ESP_LOGI(TAG, "/handoff - BLE suspended");
    } else {
        ESP_LOGW(TAG, "/handoff - WiFi already ended; BLE stays up");
    }
    xSemaphoreGive(s_stop_lock);
    log_heap("handoff");
    vTaskDelete(NULL);
}

static esp_err_t handoff_post_handler(httpd_req_t *req)
{
    touch_http_activity();
    httpd_resp_sendstr(req, "ok");
    ESP_LOGI(TAG, "/handoff - dropping BLE (radio handoff)");
    if (idl0_ble_is_suspended()) {
        return ESP_OK;  /* idempotent */
    }
    if (xTaskCreate(ble_suspend_task, "ble_suspend", 4096, NULL, 4, NULL) != pdPASS) {
        ESP_LOGW(TAG, "/handoff - suspend task create failed; BLE stays up");
    }
    return ESP_OK;
}

/* One-shot timer for deferred AP teardown, shared by POST /wifi_off and
 * idl0_wifi_stop_deferred() (T11). Runs in the esp_timer task — NOT the
 * httpd task, and NOT the BLE/NimBLE host task — because httpd_stop()
 * joins the httpd task and would deadlock/stall its caller if invoked
 * directly from either. Created once in idl0_wifi_start() (see
 * ensure_wifi_off_timer below), reused across WiFi sessions. */
static esp_timer_handle_t s_wifi_off_timer = NULL;

static void wifi_off_timer_cb(void *arg)
{
    (void)arg;
    idl0_wifi_stop();
    /* BLE is back (idl0_wifi_stop resumes it) — push a status frame so a
     * reconnecting app sees WiFi: OFF immediately. */
    idl0_status_publish();
}

/* Creates s_wifi_off_timer if it doesn't exist yet. Called exactly once
 * per boot, from idl0_wifi_start() before httpd_start() — NOT lazily
 * from arm_wifi_off_timer() below. That used to be lazy-in-arm, but
 * arm_wifi_off_timer() is now reachable from two tasks (the httpd worker
 * via POST /wifi_off, and the NimBLE host task via
 * idl0_wifi_stop_deferred(), T11): a first-call race between them could
 * create two esp_timer handles and leak/orphan one. idl0_wifi_start() is
 * only ever invoked from the single task that owns CMD_WIFI_ON (main.c's
 * BLE command dispatcher, itself serialized), so creation here has
 * exactly one caller and needs no additional lock. */
static void ensure_wifi_off_timer(void)
{
    if (s_wifi_off_timer != NULL) return;
    const esp_timer_create_args_t args = {
        .callback = wifi_off_timer_cb,
        .name     = "wifi_off",
    };
    esp_err_t err = esp_timer_create(&args, &s_wifi_off_timer);
    if (err != ESP_OK) {
        /* Non-fatal to AP/HTTP bring-up: POST /wifi_off and
         * idl0_wifi_stop_deferred() both degrade to a clean error via
         * arm_wifi_off_timer()'s NULL check below instead of a crash —
         * WiFi mode just can't be torn down deferred until next boot. */
        ESP_LOGW(TAG, "wifi_off timer create failed: %s", esp_err_to_name(err));
    }
}

/* Arms the deferred-stop timer for a callback 500 ms out. Arms only —
 * does NOT create; see ensure_wifi_off_timer() above for why creation
 * moved to idl0_wifi_start()'s single-threaded path.
 *
 * s_wifi_off_timer == NULL here means either ensure_wifi_off_timer()'s
 * create failed (logged there) or WiFi was never started this boot at
 * all (e.g. BLE CMD_WIFI_OFF arriving while already IDL0_WIFI_OFF, so
 * idl0_wifi_start() never ran and never created the timer). Either way
 * there is nothing to defer — idl0_wifi_stop() would itself no-op on an
 * already-OFF state — so this returns an error instead of dereferencing
 * a NULL handle, rather than crashing. idl0_wifi_stop_deferred() below
 * treats that error as a harmless no-op; wifi_off_post_handler still
 * surfaces it as a 500 (pre-existing behaviour, unchanged by this fix —
 * POST /wifi_off can only be reached once httpd is up, which requires
 * idl0_wifi_start() to have already run ensure_wifi_off_timer(), so in
 * practice that path only 500s if timer creation itself failed at
 * startup).
 *
 * Idempotence (for the case the timer does exist): esp_timer_start_once()
 * on a one-shot timer that is already armed returns
 * ESP_ERR_INVALID_STATE without re-arming or re-queuing it, so two stops
 * queued back-to-back (e.g. BLE CMD_WIFI_OFF racing a POST /wifi_off, or
 * either one arriving twice) collapse onto the single pending callback —
 * treated here as success, not an error. idl0_wifi_stop() itself is also
 * idempotent (it short-circuits when already IDL0_WIFI_OFF), so even a
 * spurious extra fire of the callback is a no-op. */
static esp_err_t arm_wifi_off_timer(void)
{
    if (s_wifi_off_timer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = esp_timer_start_once(s_wifi_off_timer, 500 * 1000);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    return ESP_OK;
}

/* T11: defers idl0_wifi_stop() to the esp_timer task instead of running
 * it on the caller's task. main.c's IDL0_CMD_WIFI_OFF case calls this
 * from the NimBLE host task, where a direct idl0_wifi_stop() would block
 * on httpd_stop() joining the httpd worker — stalling ALL BLE command
 * processing for however long an in-flight handler (large /download, a
 * stalled /ota) takes to finish. See arm_wifi_off_timer() for the
 * idempotence guarantee covering repeated/racing calls, and for why a
 * NULL timer (WiFi never started this boot) is a safe no-op here rather
 * than an error worth surfacing — idl0_wifi_stop() has nothing to do in
 * that case either. */
void idl0_wifi_stop_deferred(void)
{
    esp_err_t err = arm_wifi_off_timer();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "idl0_wifi_stop_deferred: timer arm failed: %s",
                 esp_err_to_name(err));
    }
}

/* POST /wifi_off — exits WiFi mode over HTTP; the normal exit path once
 * BLE is off (SPEC §6.1, §10.4). The 500 ms delay lets the 200 flush
 * before the AP drops (same pattern as /config's reboot). */
static esp_err_t wifi_off_post_handler(httpd_req_t *req)
{
    touch_http_activity();

    if (arm_wifi_off_timer() != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "timer create failed");
        return ESP_FAIL;
    }

    httpd_resp_sendstr(req, "ok");
    ESP_LOGI(TAG, "/wifi_off - AP teardown in 500 ms");
    return ESP_OK;
}

/* §10.4 no-activity failsafe: five minutes with no associated station,
 * or five minutes with no HTTP request, exits WiFi mode autonomously so
 * the device is never stranded in AP mode draining battery. The app's
 * 10 s /ping heartbeat keeps an active link alive indefinitely. */
#define IDL0_WIFI_IDLE_TIMEOUT_US   (5LL * 60 * 1000 * 1000)
#define IDL0_WIFI_FAILSAFE_POLL_MS  10000

static TaskHandle_t s_failsafe_task = NULL;

/* Last time at least one station was associated (or AP-up time). */
static int64_t s_last_sta_us = 0;

static void failsafe_task(void *arg)
{
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(IDL0_WIFI_FAILSAFE_POLL_MS));

        wifi_sta_list_t stas = {0};
        if (esp_wifi_ap_get_sta_list(&stas) == ESP_OK && stas.num > 0) {
            s_last_sta_us = esp_timer_get_time();
        }

        int64_t now = esp_timer_get_time();
        bool sta_idle  = (now - s_last_sta_us)  > IDL0_WIFI_IDLE_TIMEOUT_US;
        bool http_idle = (now - s_last_http_us) > IDL0_WIFI_IDLE_TIMEOUT_US;
        if (sta_idle || http_idle) {
            ESP_LOGW(TAG, "no-activity failsafe (%s) - exiting WiFi mode",
                     sta_idle ? "no station for 5 min" : "no HTTP for 5 min");
            /* Clear the handle first so idl0_wifi_stop doesn't try to
             * delete the task it is running on; we self-delete below. */
            s_failsafe_task = NULL;
            idl0_wifi_stop();
            idl0_status_publish();
            vTaskDelete(NULL);
        }
    }
}

static esp_err_t register_endpoints(httpd_handle_t srv)
{
    static const httpd_uri_t files_uri = {
        .uri = "/files", .method = HTTP_GET, .handler = files_get_handler,
    };
    static const httpd_uri_t download_uri = {
        .uri = "/download", .method = HTTP_GET, .handler = download_get_handler,
    };
    static const httpd_uri_t delete_uri = {
        .uri = "/delete", .method = HTTP_GET, .handler = delete_get_handler,
    };
    static const httpd_uri_t config_uri = {
        .uri = "/config", .method = HTTP_POST, .handler = config_post_handler,
    };
    static const httpd_uri_t ota_uri = {
        .uri = "/ota", .method = HTTP_POST, .handler = ota_post_handler,
    };
    static const httpd_uri_t ping_uri = {
        .uri = "/ping", .method = HTTP_GET, .handler = ping_get_handler,
    };
    static const httpd_uri_t handoff_uri = {
        .uri = "/handoff", .method = HTTP_POST, .handler = handoff_post_handler,
    };
    static const httpd_uri_t wifi_off_uri = {
        .uri = "/wifi_off", .method = HTTP_POST, .handler = wifi_off_post_handler,
    };
    esp_err_t err = httpd_register_uri_handler(srv, &files_uri);
    if (err == ESP_OK) err = httpd_register_uri_handler(srv, &download_uri);
    if (err == ESP_OK) err = httpd_register_uri_handler(srv, &delete_uri);
    if (err == ESP_OK) err = httpd_register_uri_handler(srv, &config_uri);
    if (err == ESP_OK) err = httpd_register_uri_handler(srv, &ota_uri);
    if (err == ESP_OK) err = httpd_register_uri_handler(srv, &ping_uri);
    if (err == ESP_OK) err = httpd_register_uri_handler(srv, &handoff_uri);
    if (err == ESP_OK) err = httpd_register_uri_handler(srv, &wifi_off_uri);
    return err;
}

static bool ensure_event_loop(void)
{
    if (s_event_loop_inited) return true;
    esp_err_t err = esp_event_loop_create_default();
    if (err == ESP_ERR_INVALID_STATE) {
        /* Already created elsewhere — fine. */
        s_event_loop_inited = true;
        return true;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_event_loop_create_default: %s", esp_err_to_name(err));
        return false;
    }
    s_event_loop_inited = true;
    return true;
}

static bool ensure_netif(void)
{
    static bool netif_inited = false;
    if (netif_inited) return true;
    esp_err_t err = esp_netif_init();
    if (err == ESP_ERR_INVALID_STATE) {
        netif_inited = true;
        return true;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_init: %s", esp_err_to_name(err));
        return false;
    }
    netif_inited = true;
    return true;
}

bool idl0_wifi_start(void)
{
    if (s_state == IDL0_WIFI_ON) {
        return true;
    }
    if (s_stop_lock == NULL) {
        s_stop_lock = xSemaphoreCreateMutex();
        if (s_stop_lock == NULL) {
            ESP_LOGE(TAG, "stop lock create failed");
            return false;
        }
    }
    s_abort_transfers = false;
    log_heap("wifi start");
    /* Give the idle recording buffer to WiFi for the length of WiFi mode. */
    idl0_writer_release();
    if (!ensure_netif() || !ensure_event_loop()) {
        return false;
    }
    if (s_ap_netif == NULL) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
        if (s_ap_netif == NULL) {
            ESP_LOGE(TAG, "esp_netif_create_default_wifi_ap failed");
            return false;
        }
    }

    if (!s_wifi_inited) {
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_err_t err = esp_wifi_init(&cfg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_wifi_init: %s", esp_err_to_name(err));
            return false;
        }
        s_wifi_inited = true;
    }

    /* Build the AP config. SSID comes from idl0_device_name() — "IDL1-XXXX". */
    const char *ssid = idl0_device_name();
    wifi_config_t wifi_cfg = {0};
    wifi_cfg.ap.ssid_len       = (uint8_t)strnlen(ssid, sizeof(wifi_cfg.ap.ssid));
    wifi_cfg.ap.channel        = IDL0_WIFI_CHANNEL;
    wifi_cfg.ap.max_connection = IDL0_WIFI_MAX_CONN;
    wifi_cfg.ap.authmode       = WIFI_AUTH_WPA2_PSK;
    memcpy(wifi_cfg.ap.ssid, ssid, wifi_cfg.ap.ssid_len);
    strncpy((char *)wifi_cfg.ap.password, IDL0_WIFI_PASSWORD,
            sizeof(wifi_cfg.ap.password) - 1);

    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err == ESP_OK) err = esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg);
    if (err == ESP_OK) err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "AP start failed: %s", esp_err_to_name(err));
        return false;
    }
    /* AP-mode does not sleep; set explicitly for clarity. */
    esp_wifi_set_ps(WIFI_PS_NONE);

    /* HTTP server. Endpoint handlers register in later milestones. */
    httpd_config_t hcfg = HTTPD_DEFAULT_CONFIG();
    hcfg.max_uri_handlers = 12;
    /* The default httpd task stack (4 KB) overflows: an 8 KB read
     * buffer in /download + ~256 B of locals + httpd/lwIP call-frame
     * depth needs ~12-14 KB. 16 KB leaves a comfortable margin. */
    hcfg.stack_size = 16384;
    /* OTA uploads ~1.5 MB; even at 200 KB/s that's ~8 s end-to-end,
     * and any single TCP stall under load can pause a chunk for
     * several seconds. The 5 s default trips during normal uploads. */
    hcfg.recv_wait_timeout = 30;
    hcfg.send_wait_timeout = 30;
    /* Without lru_purge_enable, half-open sockets (client vanished
     * without FIN/RST) accumulate up to max_open_sockets and every new
     * connection — including the app's own reconnect — is then refused
     * until one times out on its own. LRU-evict the stalest instead so a
     * fresh connection always wins. 4 sockets bounds memory (each socket
     * backs a receive buffer) while covering handoff + reconnect. */
    hcfg.lru_purge_enable  = true;
    hcfg.max_open_sockets  = 4;

    /* Create the deferred-stop timer before the httpd server starts, on
     * this single-threaded call path (T11 concurrency fix) — so that by
     * the time POST /wifi_off can possibly be reached, s_wifi_off_timer
     * is already non-NULL. See ensure_wifi_off_timer()'s doc comment for
     * why this can no longer be done lazily inside arm_wifi_off_timer(). */
    ensure_wifi_off_timer();

    err = httpd_start(&s_httpd, &hcfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start: %s", esp_err_to_name(err));
        esp_wifi_stop();
        return false;
    }
    esp_err_t reg_err = register_endpoints(s_httpd);
    if (reg_err != ESP_OK) {
        ESP_LOGE(TAG, "register_endpoints: %s", esp_err_to_name(reg_err));
        httpd_stop(s_httpd);
        s_httpd = NULL;
        esp_wifi_stop();
        return false;
    }

    /* Arm the §10.4 no-activity failsafe: both idle windows start at
     * AP-up so a phone that never joins still gets timed out. */
    s_last_http_us = esp_timer_get_time();
    s_last_sta_us  = s_last_http_us;
    if (xTaskCreate(failsafe_task, "wifi_failsafe", 4096, NULL, 3,
                    &s_failsafe_task) != pdPASS) {
        ESP_LOGW(TAG, "failsafe task create failed - no idle timeout");
        s_failsafe_task = NULL;
    }

    s_state = IDL0_WIFI_ON;
    idl0_mode_set_bits(IDL0_MODE_BIT_WIFI_UP);
    ESP_LOGI(TAG, "AP up: SSID=%s, IP=192.168.4.1, HTTP on :80", ssid);
    log_heap("AP up");
    idl0_diag_log_event("wifi up");
    return true;
}

bool idl0_wifi_stop(void)
{
    ESP_LOGI(TAG, "wifi_stop enter; state=%s", idl0_wifi_state_str(s_state));
    if (s_stop_lock == NULL) {
        return true;  /* WiFi never started this boot */
    }
    xSemaphoreTake(s_stop_lock, portMAX_DELAY);
    if (s_state == IDL0_WIFI_OFF) {
        xSemaphoreGive(s_stop_lock);
        return true;
    }
    s_abort_transfers = true;
    /* Kill the failsafe poller — unless the failsafe itself is the
     * caller (it cleared the handle and self-deletes after we return). */
    if (s_failsafe_task != NULL &&
        xTaskGetCurrentTaskHandle() != s_failsafe_task) {
        vTaskDelete(s_failsafe_task);
    }
    s_failsafe_task = NULL;
    if (s_httpd != NULL) {
        httpd_stop(s_httpd);
        s_httpd = NULL;
    }
    esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_stop: %s", esp_err_to_name(err));
    }
    idl0_mode_clear_bits(IDL0_MODE_BIT_WIFI_UP);
    s_state = IDL0_WIFI_OFF;
    /* WiFi's memory is back: rebuild the recording buffer before anything
     * can start a session. */
    idl0_writer_restore();
    log_heap("AP down");
    /* §10.4: BLE comes back whenever WiFi mode ends, regardless of which
     * path ended it (BLE CMD_WIFI_OFF pre-handoff, POST /wifi_off, or
     * the no-activity failsafe). No-op if BLE was never suspended. */
    idl0_ble_resume();
    xSemaphoreGive(s_stop_lock);
    ESP_LOGI(TAG, "AP down");
    idl0_diag_log_event("wifi down");
    return true;
}
