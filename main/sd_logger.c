/* SD card SPI / FATFS mount and presence state.
 *
 * P5 Milestone 1: mount + free-space classification. The append-only
 * file writer (idl0_sd_open_session / _append / _rename / _close) is
 * added in Milestone 4.
 *
 * See IDL0_SPEC.md §10.2 for the SD layout contract (spec location: docs/README.md).
 */

#include "sd_logger.h"

#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "pins.h"

static const char *TAG = "sd";

static sdmmc_card_t   *s_card  = NULL;
static idl0_sd_state_t s_state = IDL0_SD_ABSENT;
static FILE             *s_file       = NULL;
static SemaphoreHandle_t s_file_mutex = NULL;
static char              s_file_path[64] = {0};

/* Full-buffering for the session file. 16 KB matches the FAT allocation-unit
 * size (see mount_cfg below), so fwrite accumulates a whole cluster before
 * stdio writes it through to the card. This replaces the per-append fflush
 * that forced a synchronous, often sub-cluster, SD write ~60×/s and back-
 * pressured the shared SPI bus into IMU FIFO overruns. Durability is bounded
 * by the writer task's ~1 Hz idl0_sd_flush() and the session-close flush.
 * Static (one session file open at a time) — avoids per-session heap churn. */
#define IDL0_SD_IO_BUF_BYTES (16 * 1024)
static char s_io_buf[IDL0_SD_IO_BUF_BYTES];

/* T04/A7 capacity cache (§10.2). esp_vfs_fat_info() walks f_getfree() over
 * the shared SPI bus, so it must not run on every 1 Hz status tick — the
 * status publisher reads these cached values instead. Refreshed at mount,
 * session open/close, and on each successful idl0_sd_flush() (piggybacked
 * on the fsync there, which already touches the FAT). Outside an active
 * session the values may be up to one session stale; acceptable per A7. */
static uint32_t s_free_mb  = 0;
static uint32_t s_total_mb = 0;

/* Single esp_vfs_fat_info() query refreshes both cached values — it already
 * derives total bytes from the same f_getfree() call free space comes from
 * (fs->n_fatent - 2) * fs->csize * sector_size internally, so there is no
 * separate "total" query to worry about calling too often. */
static void sd_refresh_capacity_mb(void)
{
    if (s_card == NULL) {
        s_free_mb  = 0;
        s_total_mb = 0;
        return;
    }
    uint64_t total_bytes = 0, free_bytes = 0;
    esp_err_t err = esp_vfs_fat_info(IDL0_SD_MOUNT_POINT, &total_bytes, &free_bytes);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_vfs_fat_info failed: %s", esp_err_to_name(err));
        return;
    }
    s_free_mb  = (uint32_t)(free_bytes  / (1024ULL * 1024ULL));
    s_total_mb = (uint32_t)(total_bytes / (1024ULL * 1024ULL));
}

idl0_sd_state_t idl0_sd_state(void) { return s_state; }

const char *idl0_sd_state_str(idl0_sd_state_t state)
{
    switch (state) {
        case IDL0_SD_OK:    return "OK";
        case IDL0_SD_FULL:  return "FULL";
        case IDL0_SD_ERROR: return "ERROR";
        case IDL0_SD_ABSENT:
        default:            return "ABSENT";
    }
}

uint32_t idl0_sd_free_mb(void)
{
    return s_free_mb;
}

uint32_t idl0_sd_total_mb(void)
{
    return s_total_mb;
}

bool idl0_sd_init(spi_host_device_t spi_host)
{
    /* Shared SPI bus — also used by the IMUs (P7). Initialised here
     * because the SD card is the first device on it. */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = IDL0_PIN_SPI_MOSI,
        .miso_io_num     = IDL0_PIN_SPI_MISO,
        .sclk_io_num     = IDL0_PIN_SPI_SCK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 4096,
    };
    esp_err_t err = spi_bus_initialize(spi_host, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(err));
        s_state = IDL0_SD_ABSENT;
        return false;
    }

    /* format_if_mount_failed = false — never silently format a user's
     * card; report ABSENT instead. */
    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files              = 4,
        .allocation_unit_size   = 16 * 1024,
    };

    sdspi_device_config_t slot_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_cfg.gpio_cs  = IDL0_PIN_CS_SD;
    slot_cfg.host_id  = spi_host;

    sdmmc_host_t host  = SDSPI_HOST_DEFAULT();
    host.slot          = spi_host;

    err = esp_vfs_fat_sdspi_mount(IDL0_SD_MOUNT_POINT, &host, &slot_cfg,
                                  &mount_cfg, &s_card);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "SD mount failed (%s) — no card or unformatted",
                 esp_err_to_name(err));
        s_card  = NULL;
        s_state = IDL0_SD_ABSENT;
        return false;
    }

    sd_refresh_capacity_mb();   /* mount: prime both cached values (§ above) */
    s_state = (s_free_mb >= IDL0_SD_MIN_FREE_MB) ? IDL0_SD_OK : IDL0_SD_FULL;
    ESP_LOGI(TAG, "SD mounted: %lu/%lu MB free/total, state %s",
             (unsigned long)s_free_mb, (unsigned long)s_total_mb,
             idl0_sd_state_str(s_state));
    return true;
}

bool idl0_sd_open_session(int64_t boot_ms)
{
    if (s_state != IDL0_SD_OK && s_state != IDL0_SD_FULL) {
        ESP_LOGE(TAG, "open_session: SD not mounted");
        return false;
    }
    if (s_file_mutex == NULL) {
        s_file_mutex = xSemaphoreCreateMutex();
        if (s_file_mutex == NULL) {
            ESP_LOGE(TAG, "open_session: mutex alloc failed");
            return false;
        }
    }
    /* T02: a stray handle here (e.g. a caller that skipped
     * idl0_sd_close_session()/idl0_sd_discard_session() on the previous
     * session) leaks a FATFS file descriptor. max_files = 4 above means four
     * leaked handles make every later fopen fail until reboot. Defence-in-
     * depth close — the race that could produce a stray handle in the first
     * place is tracked and accepted separately. Same mutex as the rest of
     * this file's s_file access. */
    if (xSemaphoreTake(s_file_mutex, portMAX_DELAY) == pdTRUE) {
        if (s_file != NULL) {
            ESP_LOGW(TAG, "open_session: closing stray pre-existing file handle");
            fflush(s_file);
            fclose(s_file);
            s_file = NULL;
        }
        xSemaphoreGive(s_file_mutex);
    }

    /* Ensure /sessions exists. mkdir on an existing dir returns an error
     * we deliberately ignore. */
    mkdir(IDL0_SD_MOUNT_POINT "/sessions", 0777);

    snprintf(s_file_path, sizeof(s_file_path),
             IDL0_SD_MOUNT_POINT "/sessions/tmp_%lld.idl0", (long long)boot_ms);
    s_file = fopen(s_file_path, "wb");
    if (s_file == NULL) {
        ESP_LOGE(TAG, "open_session: fopen %s failed", s_file_path);
        s_state = IDL0_SD_ERROR;
        return false;
    }
    setvbuf(s_file, s_io_buf, _IOFBF, sizeof(s_io_buf));   /* cluster-sized writes */
    ESP_LOGI(TAG, "session file: %s", s_file_path);
    sd_refresh_capacity_mb();   /* session start: cache refresh point (§ above) */
    return true;
}

bool idl0_sd_append(const uint8_t *buf, size_t len)
{
    if (s_file == NULL) {
        return false;
    }
    bool ok = false;
    if (xSemaphoreTake(s_file_mutex, portMAX_DELAY) == pdTRUE) {
        /* fwrite into the 16 KB stdio buffer only — no fflush here. stdio
         * writes through to the card a full cluster at a time, and the writer
         * task flushes on a ~1 Hz cadence (idl0_sd_flush) for durability. This
         * is what unblocks the shared SPI bus for the IMU drain. */
        size_t written = fwrite(buf, 1, len, s_file);
        if (written == len) {
            ok = true;
        } else {
            ESP_LOGE(TAG, "append: short write (%u/%u)",
                     (unsigned)written, (unsigned)len);
            s_state = IDL0_SD_ERROR;
        }
        xSemaphoreGive(s_file_mutex);
    }
    return ok;
}

bool idl0_sd_flush(void)
{
    if (s_file == NULL) {
        return false;
    }
    bool ok = false;
    if (xSemaphoreTake(s_file_mutex, portMAX_DELAY) == pdTRUE) {
        /* Two-step durability commit under the file mutex:
         *   fflush — push the 16 KB stdio buffer through to FATFS (writes the
         *            pending data clusters to the card).
         *   fsync  — force FATFS to write back the directory entry (the file
         *            SIZE + last cluster) and the dirty FAT window.
         * The fsync is the part that matters for recovery: without it FATFS only
         * commits the on-disk size at f_close (the first-fix rename and session
         * close), so a power loss in between freezes the directory size — the
         * sample clusters reach the card but the file reads as truncated at the
         * last close. fsync at the writer's ~1 Hz cadence bounds that freeze to
         * one flush interval.
         *
         * Hot-loop-safe: runs only on the writer task, never the IMU task,
         * behind the mutex append already uses (the IMU drain hands off via the
         * lock-free ring buffer and never takes it). The SD writes block the
         * writer on DMA-completion, yielding the single core to the drain; the
         * only shared resource is the SPI2 bus (IMU0 + SD), serialized one
         * ~512 B transaction at a time. fsync adds ~2 metadata sectors on top of
         * the cluster write fflush already does each second. */
        if (s_file != NULL && fflush(s_file) == 0 && fsync(fileno(s_file)) == 0) {
            ok = true;
        } else if (s_file != NULL) {
            s_state = IDL0_SD_ERROR;
        }
        xSemaphoreGive(s_file_mutex);
    }
    if (ok) {
        /* Cache refresh piggybacked on the fsync above — the FAT is already
         * being touched here, so this is the ~1 Hz cadence the capacity
         * cache rides during a session, not an extra SPI-bus hit (§ above). */
        sd_refresh_capacity_mb();
    }
    return ok;
}

bool idl0_sd_rename_with_timestamp(int64_t utc_seconds)
{
    if (s_file == NULL) {
        return false;
    }
    time_t t = (time_t)utc_seconds;
    struct tm tm_utc;
    gmtime_r(&t, &tm_utc);
    char new_path[64];
    strftime(new_path, sizeof(new_path),
             IDL0_SD_MOUNT_POINT "/sessions/%Y-%m-%d_%H-%M-%S.idl0", &tm_utc);

    bool ok = false;
    if (xSemaphoreTake(s_file_mutex, portMAX_DELAY) == pdTRUE) {
        fflush(s_file);
        fclose(s_file);
        s_file = NULL;  /* null until reopen — no dangling pointer */
        if (rename(s_file_path, new_path) == 0) {
            snprintf(s_file_path, sizeof(s_file_path), "%s", new_path);
            ok = true;
        } else {
            ESP_LOGE(TAG, "rename to %s failed", new_path);
        }
        s_file = fopen(s_file_path, "ab");  /* reopen for continued append */
        if (s_file != NULL) {
            setvbuf(s_file, s_io_buf, _IOFBF, sizeof(s_io_buf));  /* keep cluster buffering */
        }
        xSemaphoreGive(s_file_mutex);
    }
    if (ok) {
        ESP_LOGI(TAG, "session file renamed: %s", s_file_path);
    }
    return ok && s_file != NULL;
}

bool idl0_sd_close_session(void)
{
    if (s_file == NULL) {
        return false;
    }
    bool ok;
    if (xSemaphoreTake(s_file_mutex, portMAX_DELAY) == pdTRUE) {
        fflush(s_file);
        ok = (fclose(s_file) == 0);
        s_file = NULL;
        xSemaphoreGive(s_file_mutex);
    } else {
        ok = false;
    }
    sd_refresh_capacity_mb();   /* session stop: cache refresh point (§ above) */
    ESP_LOGI(TAG, "session file closed");
    return ok;
}

void idl0_sd_discard_session(void)
{
    if (s_file_mutex != NULL && xSemaphoreTake(s_file_mutex, portMAX_DELAY) == pdTRUE) {
        /* T44: no fflush — the file is being discarded, not committed. */
        if (s_file != NULL) {
            fclose(s_file);
            s_file = NULL;
        }
        if (s_file_path[0] != '\0') {
            remove(s_file_path);
            s_file_path[0] = '\0';
        }
        xSemaphoreGive(s_file_mutex);
    }
    ESP_LOGW(TAG, "session file discarded");
}
