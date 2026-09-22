/* See ota_guard.h. Pure C99 — no ESP-IDF includes. */

#include "ota_guard.h"

#include <string.h>

static uint32_t rd_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

idl1_ota_guard_result_t idl1_ota_guard_check(const uint8_t *buf, size_t len,
                                             const char *expected_project,
                                             char *got, size_t got_len)
{
    if (got != NULL && got_len > 0) got[0] = '\0';
    if (buf == NULL || len < IDL1_OTA_GUARD_HDR_BYTES) {
        return IDL1_OTA_GUARD_NEED_MORE;
    }
    if (buf[0] != IDL1_OTA_IMAGE_MAGIC ||
        rd_u32_le(buf + IDL1_OTA_APP_DESC_OFFSET) != IDL1_OTA_APP_DESC_MAGIC) {
        return IDL1_OTA_GUARD_BAD_IMAGE;
    }

    /* project_name is a fixed 32-byte field, NUL-padded; bound every read
     * by the field so an unterminated name cannot run on. */
    const char *name = (const char *)(buf + IDL1_OTA_PROJECT_NAME_OFFSET);
    size_t name_len = 0;
    while (name_len < IDL1_OTA_PROJECT_NAME_BYTES && name[name_len] != '\0') {
        name_len++;
    }

    if (got != NULL && got_len > 0) {
        size_t n = (name_len < got_len - 1) ? name_len : got_len - 1;
        for (size_t i = 0; i < n; i++) {
            const unsigned char c = (unsigned char)name[i];
            got[i] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
        }
        got[n] = '\0';
    }

    const size_t exp_len = (expected_project != NULL)
        ? strlen(expected_project) : 0;
    if (expected_project == NULL || exp_len != name_len ||
        memcmp(expected_project, name, name_len) != 0) {
        return IDL1_OTA_GUARD_WRONG_PROJECT;
    }
    return IDL1_OTA_GUARD_OK;
}
