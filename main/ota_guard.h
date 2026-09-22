/* OTA board guard (decisions.md F5) — pure C99, host-testable.
 *
 * The idl0 app's firmware catalog offers idl0-firmware releases, and an
 * IDL0 image on IDL1 hardware drives GPIO2 (PWR_EN) as an IMU chip-select
 * and GPIO23 (SCL) as the SD chip-select. So before anything is written to
 * flash, the OTA receive path (wifi_server.c) buffers the start of the
 * upload and checks the incoming image's esp_app_desc_t.project_name
 * against the running firmware's.
 *
 * No ESP-IDF includes here (test/test_ota_guard.c builds it on the host).
 * The offsets below mirror ESP-IDF v6.0.1's esp_app_format.h /
 * esp_app_desc.h, which static-assert the struct sizes they rely on.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/* Layout of the start of an ESP32 application image, bytes. */
#define IDL1_OTA_IMAGE_HDR_BYTES     24   /* esp_image_header_t */
#define IDL1_OTA_SEGMENT_HDR_BYTES    8   /* esp_image_segment_header_t */
#define IDL1_OTA_APP_DESC_BYTES     256   /* esp_app_desc_t */
/* Bytes the guard needs before it can decide: image header + first segment
 * header + the esp_app_desc_t that opens the first segment. */
#define IDL1_OTA_GUARD_HDR_BYTES \
    (IDL1_OTA_IMAGE_HDR_BYTES + IDL1_OTA_SEGMENT_HDR_BYTES + IDL1_OTA_APP_DESC_BYTES)

/* Field positions inside those bytes. */
#define IDL1_OTA_IMAGE_MAGIC          0xE9u        /* esp_image_header_t.magic */
#define IDL1_OTA_APP_DESC_OFFSET      (IDL1_OTA_IMAGE_HDR_BYTES + IDL1_OTA_SEGMENT_HDR_BYTES)
#define IDL1_OTA_APP_DESC_MAGIC       0xABCD5432u  /* esp_app_desc_t.magic_word, LE */
#define IDL1_OTA_PROJECT_NAME_OFFSET  (IDL1_OTA_APP_DESC_OFFSET + 48)  /* after magic, secure_version, reserv1[2], version[32] */
#define IDL1_OTA_PROJECT_NAME_BYTES   32           /* esp_app_desc_t.project_name[32] */

typedef enum {
    IDL1_OTA_GUARD_NEED_MORE = 0,  /* fewer than IDL1_OTA_GUARD_HDR_BYTES given */
    IDL1_OTA_GUARD_OK,             /* valid app image, project_name matches */
    IDL1_OTA_GUARD_BAD_IMAGE,      /* image or app-descriptor magic wrong */
    IDL1_OTA_GUARD_WRONG_PROJECT,  /* valid app image for a different project */
} idl1_ota_guard_result_t;

/* Check the first `len` bytes of an incoming OTA image against
 * `expected_project` (NUL-terminated, e.g. the running image's
 * esp_app_get_description()->project_name).
 *
 * When the result is WRONG_PROJECT or OK and `got`/`got_len` are non-NULL,
 * the image's project name is copied into `got` (NUL-terminated, truncated
 * to got_len - 1, non-printable bytes replaced by '?') for the error
 * message. `got` is set to "" otherwise. Never reads past buf[len - 1]. */
idl1_ota_guard_result_t idl1_ota_guard_check(const uint8_t *buf, size_t len,
                                             const char *expected_project,
                                             char *got, size_t got_len);
