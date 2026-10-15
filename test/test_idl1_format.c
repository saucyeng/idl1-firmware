/* Host-compiled unit test for idl1_format.c (the .idl1 encoders, spec
 * docs/superpowers/specs/2026-10-15-idl1-file-format-DRAFT.md in idl1-app).
 *
 * Build & run (from test/):
 *   cc -std=c99 -I../main test_idl1_format.c ../main/idl1_format.c ../main/idl0_format.c -o test_idl1_format
 *   ./test_idl1_format
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "idl0_format.h"
#include "idl1_format.h"

static int g_checks = 0;
#define CHECK(cond) do { g_checks++; if (!(cond)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); return 1; } } while (0)

static uint16_t rd_u16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int64_t rd_i64(const uint8_t *p)
{
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) v = (v << 8) | p[i];
    return (int64_t)v;
}

static int test_header(void)
{
    idl1_registry_entry_t reg[2];
    memset(reg, 0, sizeof(reg));
    reg[0] = (idl1_registry_entry_t){ .channel_id = 0, .data_type = 4, .kind = IDL1_KIND_IMU_AXIS,
        .source = 0, .axis = 0, .path = IDL1_PATH_LOW_G, .rate_mhz = 480000, .scale = 0.000488f };
    strcpy(reg[0].name, "IMU0_AccelX");
    strcpy(reg[0].units, "g");
    reg[1] = (idl1_registry_entry_t){ .channel_id = IDL1_CH_HIGH_G_BASE + 3, .data_type = 4,
        .kind = IDL1_KIND_IMU_AXIS, .source = 1, .axis = 0, .path = IDL1_PATH_HIGH_G, .rate_mhz = 960000,
        .scale = 0.001952f };
    strcpy(reg[1].name, "IMU1_HgAccelX");
    strcpy(reg[1].units, "g");
    uint8_t uuid[16], dev[6];
    for (int i = 0; i < 16; i++) uuid[i] = (uint8_t)(i + 1);
    for (int i = 0; i < 6; i++) dev[i] = (uint8_t)(0xA0 + i);
    const int8_t ff[3] = { -2, 0, 5 };
    uint8_t buf[512];

    size_t n = idl1_format_header(buf, sizeof(buf), uuid, dev, "0.3.0", 1760000000000LL, 0xCAFEBABE, ff, 2, reg);

    CHECK(n == idl1_header_size(2));
    CHECK(n == 73 + 2 * 52 + 4);
    CHECK(memcmp(buf, "IDL1", 4) == 0);
    CHECK(rd_u16(buf + 4) == IDL1_FORMAT_VERSION);
    CHECK(rd_u32(buf + 6) == n);
    CHECK(buf[10] == 1 && buf[25] == 16);              /* uuid */
    CHECK(buf[26] == 0xA0 && buf[31] == 0xA5);         /* device id */
    CHECK(memcmp(buf + 32, "0.3.0", 6) == 0);          /* firmware, NUL-terminated */
    CHECK(rd_i64(buf + 56) == 1760000000000LL);        /* = the parser's peek offset */
    CHECK(rd_u32(buf + 64) == 0xCAFEBABE);
    CHECK((int8_t)buf[68] == -2 && buf[70] == 5);
    CHECK(rd_u16(buf + 71) == 2);
    const uint8_t *e1 = buf + 73 + 52;
    CHECK(rd_u16(e1) == IDL1_CH_HIGH_G_BASE + 3);
    CHECK(e1[2] == 4 && e1[3] == IDL1_KIND_IMU_AXIS && e1[4] == 1 && e1[5] == 0 && e1[6] == IDL1_PATH_HIGH_G);
    CHECK(rd_u32(e1 + 8) == 960000);
    CHECK(memcmp(e1 + 20, "IMU1_HgAccelX", 14) == 0);
    CHECK(memcmp(e1 + 44, "g", 2) == 0);
    CHECK(rd_u32(buf + n - 4) == idl0_format_crc32(buf, n - 4));
    CHECK(idl1_format_header(buf, n - 1, uuid, dev, "0.3.0", 0, 0, ff, 2, reg) == 0);
    return 0;
}

static int test_imu_block(void)
{
    const int16_t samples[2][6] = { { 1, 2, 3, 4, 5, 6 }, { -1, -2, -3, -4, -5, -6 } };
    const int16_t hg[3][3] = { { 10, 11, 12 }, { 13, 14, 15 }, { 16, 17, 18 } };
    const idl1_ts_word_t ts[1] = { { .after_sample = 1, .after_hg = 2, .chip_ts = 0x01020304u } };
    uint8_t buf[256];

    size_t n = idl1_format_imu_block(buf, sizeof(buf), 2, 1, 123456789LL, 0xAABBCCDDu,
                                     &samples[0][0], 2, 6, &hg[0][0], 3, 3, ts, 1);

    CHECK(n == idl1_imu_block_size(2, 6, 3, 3, 1));
    CHECK(n == 3 + 19 + 24 + 18 + 8);
    CHECK(buf[0] == IDL1_REC_IMU_BLOCK && rd_u16(buf + 1) == n - 3);
    CHECK(buf[3] == 2 && buf[4] == 1);
    CHECK(rd_i64(buf + 5) == 123456789LL);
    CHECK(rd_u32(buf + 13) == 0xAABBCCDDu);
    CHECK(rd_u16(buf + 17) == 2 && rd_u16(buf + 19) == 3 && buf[21] == 1);
    CHECK((int16_t)rd_u16(buf + 22) == 1 && (int16_t)rd_u16(buf + 22 + 22) == -6);
    CHECK((int16_t)rd_u16(buf + 46) == 10 && (int16_t)rd_u16(buf + 46 + 16) == 18);
    CHECK(rd_u16(buf + 64) == 1 && rd_u16(buf + 66) == 2 && rd_u32(buf + 68) == 0x01020304u);
    CHECK(idl1_format_imu_block(buf, n - 1, 2, 1, 0, 0, &samples[0][0], 2, 6, &hg[0][0], 3, 3, ts, 1) == 0);
    return 0;
}

static int test_small_records(void)
{
    uint8_t buf[64];

    size_t g = idl1_format_gps_fix(buf, 1760000123000LL, 1000000, 515250000, -1234567, 500, 1000, 18000, 1, 9, 85);
    CHECK(g == 3 + IDL1_GPS_FIX_PAYLOAD);
    CHECK(buf[0] == IDL1_REC_GPS_FIX && rd_u16(buf + 1) == 34);
    CHECK(rd_u16(buf + 3 + 32) == 85);

    const uint32_t pulses = 7;
    size_t c = idl1_format_channel_sample(buf, IDL1_CH_WHEEL_FRONT, 1500000, &pulses, 4);
    CHECK(c == 3 + 2 + 8 + 4);
    CHECK(buf[0] == IDL1_REC_CHANNEL_SAMPLE && rd_u16(buf + 3) == 32 && rd_i64(buf + 5) == 1500000);
    CHECK(rd_u32(buf + 13) == 7);
    CHECK(idl1_format_channel_sample(buf, 1, 0, &pulses, 9) == 0);

    size_t e = idl1_format_logger_event(buf, IDL1_EVT_WRITER_DROP, 0, 3, 4000000);
    CHECK(e == 18);
    CHECK(buf[3] == IDL1_EVT_WRITER_DROP && rd_u32(buf + 6) == 3 && rd_i64(buf + 10) == 4000000);

    const uint32_t ovr[3] = { 0, 2, 0 }, bus[3] = { 0, 0, 1 }, smp[3] = { 100, 200, 300 };
    size_t s = idl1_format_session_summary(buf, sizeof(buf), 3, ovr, bus, smp, 5, 1234);
    CHECK(s == 3 + 1 + 36 + 8);
    CHECK(buf[3] == 3 && rd_u32(buf + 4 + 12) == 2 && rd_u32(buf + 4 + 36) == 5);

    CHECK(idl1_format_session_end(buf) == 3 && buf[0] == IDL1_REC_SESSION_END && rd_u16(buf + 1) == 0);
    return 0;
}

int main(void)
{
    if (test_header() || test_imu_block() || test_small_records()) return 1;
    printf("PASS — %d checks\n", g_checks);
    return 0;
}
