/* Writes a golden .idl1 file with the firmware's own encoders, for the
 * idl1-app parser to read in its tests (rust/core/src/parse/testdata/
 * firmware_golden.idl1). It mirrors what a session writes: the header,
 * IMU_BLOCKs as imu_task builds them (IMU1, low-g at 480 Hz, high-g at
 * 960 Hz, a timestamp word every 32 samples, a drain every 20 ms), a GPS
 * fix, an HR sample, a logger event, the summary and the end.
 *
 * Build & run (from test/):
 *   cc -std=c99 -I../main gen_idl1_golden.c ../main/idl1_format.c ../main/idl0_format.c -o gen_idl1_golden
 *   ./gen_idl1_golden firmware_golden.idl1
 */

#include <stdio.h>
#include <string.h>

#include "idl1_format.h"

#define TICK_US   21.75
#define LOW_HZ    480
#define SECONDS   2

static uint8_t out[1 << 20];
static size_t  len = 0;

static void put(const uint8_t *b, size_t n) { memcpy(out + len, b, n); len += n; }

static void entry(idl1_registry_entry_t *e, uint16_t id, uint8_t axis, uint8_t path,
                  uint32_t rate_hz, float scale, const char *name, const char *units)
{
    memset(e, 0, sizeof(*e));
    e->channel_id = id;
    e->data_type = 4;
    e->kind = IDL1_KIND_IMU_AXIS;
    e->source = 1;
    e->axis = axis;
    e->path = path;
    e->rate_mhz = rate_hz * 1000u;
    e->scale = scale;
    strncpy(e->name, name, sizeof(e->name) - 1);
    strncpy(e->units, units, sizeof(e->units) - 1);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s out.idl1\n", argv[0]); return 2; }

    static const char *axes[6] = { "IMU1_AccelX", "IMU1_AccelY", "IMU1_AccelZ",
                                   "IMU1_GyroX", "IMU1_GyroY", "IMU1_GyroZ" };
    idl1_registry_entry_t reg[11];
    for (int a = 0; a < 6; a++)
        entry(&reg[a], (uint16_t)(6 + a), (uint8_t)a, IDL1_PATH_LOW_G, LOW_HZ,
              a < 3 ? 0.000488f : 0.07f, axes[a], a < 3 ? "g" : "dps");
    entry(&reg[6], IDL1_CH_HIGH_G_BASE + 3, 0, IDL1_PATH_HIGH_G, 960, 0.001952f, "IMU1_HgAccelX", "g");
    entry(&reg[7], IDL1_CH_HIGH_G_BASE + 4, 1, IDL1_PATH_HIGH_G, 960, 0.001952f, "IMU1_HgAccelY", "g");
    entry(&reg[8], IDL1_CH_HIGH_G_BASE + 5, 2, IDL1_PATH_HIGH_G, 960, 0.001952f, "IMU1_HgAccelZ", "g");
    memset(&reg[9], 0, sizeof(reg[9]));
    reg[9].channel_id = IDL1_CH_HR_BPM; reg[9].data_type = 0; reg[9].rate_mhz = 1000; reg[9].scale = 1.0f;
    strcpy(reg[9].name, "HR_BPM"); strcpy(reg[9].units, "bpm");
    memset(&reg[10], 0, sizeof(reg[10]));
    reg[10].channel_id = IDL1_CH_WHEEL_FRONT; reg[10].data_type = 2; reg[10].scale = 1.0f;
    strcpy(reg[10].name, "WheelFront"); strcpy(reg[10].units, "pulse");

    uint8_t uuid[16], dev[6];
    for (int i = 0; i < 16; i++) uuid[i] = (uint8_t)(0x10 + i);
    for (int i = 0; i < 6; i++) dev[i] = (uint8_t)(0xB0 + i);
    const int8_t ff[3] = { 0, -3, 0 };
    len = idl1_format_header(out, sizeof(out), uuid, dev, "0.3.0-golden", 0, 0x12345678u, ff, 11, reg);

    /* Low-g sample k at k / 480 s; high-g at twice the rate. Values: accel
     * X = k, high-g X = 2k (and its half-step neighbour 2k + 1). */
    const double period_us = 1e6 / LOW_HZ;
    int16_t samples[64][6], hg[128][3];
    idl1_ts_word_t ts[4];
    int k = 0;
    for (long read_us = 20000; k < SECONDS * LOW_HZ; read_us += 20000) {
        uint16_t n = 0, nh = 0;
        uint8_t nts = 0;
        while (k < SECONDS * LOW_HZ && k * period_us < (double)read_us) {
            if (k % 32 == 0) {
                ts[nts++] = (idl1_ts_word_t){ .after_sample = n, .after_hg = nh,
                                              .chip_ts = (uint32_t)(k * period_us / TICK_US + 0.5) };
            }
            for (int a = 0; a < 6; a++) samples[n][a] = (int16_t)(a == 0 ? k : 0);
            n++;
            for (int h = 0; h < 2; h++) {
                hg[nh][0] = (int16_t)(2 * k + h); hg[nh][1] = 0; hg[nh][2] = 0;
                nh++;
            }
            k++;
        }
        const uint32_t chip_at_read = (uint32_t)(read_us / TICK_US + 0.5);
        len += idl1_format_imu_block(out + len, sizeof(out) - len, 1, 0, read_us + 250, chip_at_read,
                                     &samples[0][0], n, 6, &hg[0][0], nh, 3, ts, nts);
    }

    uint8_t rec[64];
    put(rec, idl1_format_gps_fix(rec, 1760000500000LL, 1000000, 515250000, -1234567, 500, 1000,
                                 18000, 1, 9, 85));
    const uint8_t bpm = 142;
    put(rec, idl1_format_channel_sample(rec, IDL1_CH_HR_BPM, 1200000, &bpm, 1));
    const uint32_t pulses = 3;
    put(rec, idl1_format_channel_sample(rec, IDL1_CH_WHEEL_FRONT, 1300000, &pulses, 4));
    put(rec, idl1_format_logger_event(rec, IDL1_EVT_FIFO_OVERRUN, 1, 2, 1500000));
    const uint32_t ovr[3] = { 0, 2, 0 }, bus[3] = { 0, 0, 0 }, smp[3] = { 0, SECONDS * LOW_HZ, 0 };
    put(rec, idl1_format_session_summary(rec, sizeof(rec), 3, ovr, bus, smp, 0, 21000));
    put(rec, idl1_format_session_end(rec));

    FILE *f = fopen(argv[1], "wb");
    if (!f) { perror("open"); return 1; }
    fwrite(out, 1, len, f);
    fclose(f);
    printf("wrote %zu bytes\n", len);
    return 0;
}
