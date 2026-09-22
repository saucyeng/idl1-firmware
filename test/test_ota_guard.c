/* Host-compiled unit test for ota_guard.c (OTA board guard, decisions.md F5).
 *
 * Build & run (from test/):
 *   cc -std=c99 -I../main test_ota_guard.c ../main/ota_guard.c -o test_ota_guard
 *   ./test_ota_guard
 *
 * ota_guard.c has no ESP-IDF dependency, so any host C compiler works.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ota_guard.h"

static int g_checks = 0;
#define CHECK(cond) do { g_checks++; if (!(cond)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); return 1; } } while (0)

/* Build the first IDL1_OTA_GUARD_HDR_BYTES of a plausible app image whose
 * esp_app_desc_t.project_name is `project` (copied without a terminator if
 * it fills the 32-byte field). */
static void make_image(uint8_t *buf, const char *project)
{
    memset(buf, 0, IDL1_OTA_GUARD_HDR_BYTES);
    buf[0] = IDL1_OTA_IMAGE_MAGIC;
    buf[1] = 5;  /* segment_count — not inspected */
    uint8_t *desc = buf + IDL1_OTA_APP_DESC_OFFSET;
    desc[0] = 0x32; desc[1] = 0x54; desc[2] = 0xCD; desc[3] = 0xAB;  /* LE magic */
    memcpy(desc + 16, "1.0.0", 5);                                   /* version */
    size_t n = strlen(project);
    if (n > IDL1_OTA_PROJECT_NAME_BYTES) n = IDL1_OTA_PROJECT_NAME_BYTES;
    memcpy(buf + IDL1_OTA_PROJECT_NAME_OFFSET, project, n);
}

static int test_layout(void) {
    /* 24-byte image header + 8-byte segment header, then the app desc;
     * project_name follows magic(4) secure_version(4) reserv1(8) version(32). */
    CHECK(IDL1_OTA_GUARD_HDR_BYTES == 288);
    CHECK(IDL1_OTA_APP_DESC_OFFSET == 32);
    CHECK(IDL1_OTA_PROJECT_NAME_OFFSET == 80);
    return 0;
}

static int test_match(void) {
    uint8_t img[IDL1_OTA_GUARD_HDR_BYTES];
    char got[33];
    make_image(img, "idl1_firmware");
    CHECK(idl1_ota_guard_check(img, sizeof(img), "idl1_firmware", got, sizeof(got))
          == IDL1_OTA_GUARD_OK);
    CHECK(strcmp(got, "idl1_firmware") == 0);
    /* Extra trailing bytes are fine. */
    uint8_t big[IDL1_OTA_GUARD_HDR_BYTES + 100];
    memset(big, 0x5A, sizeof(big));
    memcpy(big, img, sizeof(img));
    CHECK(idl1_ota_guard_check(big, sizeof(big), "idl1_firmware", NULL, 0)
          == IDL1_OTA_GUARD_OK);
    return 0;
}

static int test_wrong_project(void) {
    uint8_t img[IDL1_OTA_GUARD_HDR_BYTES];
    char got[33];
    make_image(img, "idl0_firmware");
    CHECK(idl1_ota_guard_check(img, sizeof(img), "idl1_firmware", got, sizeof(got))
          == IDL1_OTA_GUARD_WRONG_PROJECT);
    CHECK(strcmp(got, "idl0_firmware") == 0);
    /* Prefix is not a match, in either direction. */
    make_image(img, "idl1_firmware_x");
    CHECK(idl1_ota_guard_check(img, sizeof(img), "idl1_firmware", NULL, 0)
          == IDL1_OTA_GUARD_WRONG_PROJECT);
    make_image(img, "idl1");
    CHECK(idl1_ota_guard_check(img, sizeof(img), "idl1_firmware", NULL, 0)
          == IDL1_OTA_GUARD_WRONG_PROJECT);
    /* Empty name. */
    make_image(img, "");
    CHECK(idl1_ota_guard_check(img, sizeof(img), "idl1_firmware", got, sizeof(got))
          == IDL1_OTA_GUARD_WRONG_PROJECT);
    CHECK(got[0] == '\0');
    return 0;
}

static int test_unterminated_and_unprintable(void) {
    uint8_t img[IDL1_OTA_GUARD_HDR_BYTES];
    char got[33];
    /* 32 chars, no NUL inside the field: must stop at the field edge. */
    make_image(img, "ABCDEFGHIJKLMNOPQRSTUVWXYZ012345");
    img[IDL1_OTA_PROJECT_NAME_OFFSET + 32] = 'Z';  /* first byte of time[] */
    CHECK(idl1_ota_guard_check(img, sizeof(img), "idl1_firmware", got, sizeof(got))
          == IDL1_OTA_GUARD_WRONG_PROJECT);
    CHECK(strlen(got) == 32);
    CHECK(memcmp(got, "ABCDEFGHIJKLMNOPQRSTUVWXYZ012345", 32) == 0);
    /* Truncation to a small output buffer. */
    char small[5];
    CHECK(idl1_ota_guard_check(img, sizeof(img), "x", small, sizeof(small))
          == IDL1_OTA_GUARD_WRONG_PROJECT);
    CHECK(strcmp(small, "ABCD") == 0);
    /* Non-printable bytes are masked in the copy. */
    make_image(img, "ab");
    img[IDL1_OTA_PROJECT_NAME_OFFSET + 1] = 0x07;
    CHECK(idl1_ota_guard_check(img, sizeof(img), "ab", got, sizeof(got))
          == IDL1_OTA_GUARD_WRONG_PROJECT);
    CHECK(strcmp(got, "a?") == 0);
    return 0;
}

static int test_need_more(void) {
    uint8_t img[IDL1_OTA_GUARD_HDR_BYTES];
    char got[33] = "junk";
    make_image(img, "idl1_firmware");
    CHECK(idl1_ota_guard_check(img, 0, "idl1_firmware", got, sizeof(got))
          == IDL1_OTA_GUARD_NEED_MORE);
    CHECK(got[0] == '\0');
    /* Even with project_name fully present (112 bytes), the guard waits for
     * the whole header + app descriptor. */
    CHECK(idl1_ota_guard_check(img, 112, "idl1_firmware", NULL, 0)
          == IDL1_OTA_GUARD_NEED_MORE);
    CHECK(idl1_ota_guard_check(img, IDL1_OTA_GUARD_HDR_BYTES - 1, "idl1_firmware", NULL, 0)
          == IDL1_OTA_GUARD_NEED_MORE);
    CHECK(idl1_ota_guard_check(NULL, 1000, "idl1_firmware", NULL, 0)
          == IDL1_OTA_GUARD_NEED_MORE);
    return 0;
}

static int test_bad_image(void) {
    uint8_t img[IDL1_OTA_GUARD_HDR_BYTES];
    make_image(img, "idl1_firmware");
    img[0] = 0x00;  /* image magic */
    CHECK(idl1_ota_guard_check(img, sizeof(img), "idl1_firmware", NULL, 0)
          == IDL1_OTA_GUARD_BAD_IMAGE);
    make_image(img, "idl1_firmware");
    img[IDL1_OTA_APP_DESC_OFFSET + 3] = 0x00;  /* app desc magic */
    CHECK(idl1_ota_guard_check(img, sizeof(img), "idl1_firmware", NULL, 0)
          == IDL1_OTA_GUARD_BAD_IMAGE);
    return 0;
}

int main(void)
{
    if (test_layout())                       return 1;
    if (test_match())                        return 1;
    if (test_wrong_project())                return 1;
    if (test_unterminated_and_unprintable()) return 1;
    if (test_need_more())                    return 1;
    if (test_bad_image())                    return 1;

    printf("PASS — %d checks\n", g_checks);
    return 0;
}
