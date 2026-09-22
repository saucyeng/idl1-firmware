/* See imu_chip_dsv320x.h. */

#include "imu_chip_dsv320x.h"

#include <string.h>

#include "esp_log.h"

static const char *TAG = "imu_chip";

/* SW_RESET completion is polled inside lsm6dsv320x_sw_reset() (3 x 1 ms).
 * Gyro turn-on after the ODR write is 40 ms (datasheet); nothing here waits
 * for it — the first FIFO words simply arrive later. */

/* FIFO tags (FIFO_DATA_OUT_TAG bits [7:3]). */
#define TAG_GY_NC   0x01u
#define TAG_XL_NC   0x02u
#define TAG_XL_HG   0x1Du   /* high-g triplet — phase 3 */

/* ---- value -> register code, valid lists only (imu_ranges.h) ------------ */

static bool odr_code(uint16_t hz, lsm6dsv320x_data_rate_t *odr,
                     lsm6dsv320x_fifo_xl_batch_t *xl_bdr,
                     lsm6dsv320x_fifo_gy_batch_t *gy_bdr)
{
    switch (hz) {
        case 15:   *odr = LSM6DSV320X_ODR_AT_15Hz;   *xl_bdr = LSM6DSV320X_XL_BATCHED_AT_15Hz;   *gy_bdr = LSM6DSV320X_GY_BATCHED_AT_15Hz;   return true;
        case 30:   *odr = LSM6DSV320X_ODR_AT_30Hz;   *xl_bdr = LSM6DSV320X_XL_BATCHED_AT_30Hz;   *gy_bdr = LSM6DSV320X_GY_BATCHED_AT_30Hz;   return true;
        case 60:   *odr = LSM6DSV320X_ODR_AT_60Hz;   *xl_bdr = LSM6DSV320X_XL_BATCHED_AT_60Hz;   *gy_bdr = LSM6DSV320X_GY_BATCHED_AT_60Hz;   return true;
        case 120:  *odr = LSM6DSV320X_ODR_AT_120Hz;  *xl_bdr = LSM6DSV320X_XL_BATCHED_AT_120Hz;  *gy_bdr = LSM6DSV320X_GY_BATCHED_AT_120Hz;  return true;
        case 240:  *odr = LSM6DSV320X_ODR_AT_240Hz;  *xl_bdr = LSM6DSV320X_XL_BATCHED_AT_240Hz;  *gy_bdr = LSM6DSV320X_GY_BATCHED_AT_240Hz;  return true;
        case 480:  *odr = LSM6DSV320X_ODR_AT_480Hz;  *xl_bdr = LSM6DSV320X_XL_BATCHED_AT_480Hz;  *gy_bdr = LSM6DSV320X_GY_BATCHED_AT_480Hz;  return true;
        case 960:  *odr = LSM6DSV320X_ODR_AT_960Hz;  *xl_bdr = LSM6DSV320X_XL_BATCHED_AT_960Hz;  *gy_bdr = LSM6DSV320X_GY_BATCHED_AT_960Hz;  return true;
        case 1920: *odr = LSM6DSV320X_ODR_AT_1920Hz; *xl_bdr = LSM6DSV320X_XL_BATCHED_AT_1920Hz; *gy_bdr = LSM6DSV320X_GY_BATCHED_AT_1920Hz; return true;
        case 3840: *odr = LSM6DSV320X_ODR_AT_3840Hz; *xl_bdr = LSM6DSV320X_XL_BATCHED_AT_3840Hz; *gy_bdr = LSM6DSV320X_GY_BATCHED_AT_3840Hz; return true;
        case 7680: *odr = LSM6DSV320X_ODR_AT_7680Hz; *xl_bdr = LSM6DSV320X_XL_BATCHED_AT_7680Hz; *gy_bdr = LSM6DSV320X_GY_BATCHED_AT_7680Hz; return true;
        default:   return false;
    }
}

static bool xl_fs_code(uint8_t g, lsm6dsv320x_xl_full_scale_t *fs)
{
    switch (g) {
        case 2:  *fs = LSM6DSV320X_2g;  return true;
        case 4:  *fs = LSM6DSV320X_4g;  return true;
        case 8:  *fs = LSM6DSV320X_8g;  return true;
        case 16: *fs = LSM6DSV320X_16g; return true;
        default: return false;
    }
}

static bool gy_fs_code(uint16_t dps, lsm6dsv320x_gy_full_scale_t *fs)
{
    switch (dps) {
        case 250:  *fs = LSM6DSV320X_250dps;  return true;
        case 500:  *fs = LSM6DSV320X_500dps;  return true;
        case 1000: *fs = LSM6DSV320X_1000dps; return true;
        case 2000: *fs = LSM6DSV320X_2000dps; return true;
        case 4000: *fs = LSM6DSV320X_4000dps; return true;
        default:   return false;
    }
}

/* ---- operating mode ----------------------------------------------------- */

lsm6dsv320x_xl_mode_t imu_chip_resolve_op_mode_xl(bool high_performance,
                                                  bool hg_enabled,
                                                  uint16_t sample_rate_hz)
{
    if (high_performance || hg_enabled) {
        /* Datasheet Table 21: with high-g on, low-g must be high-performance
         * or high-accuracy ODR. */
        return LSM6DSV320X_XL_HIGH_PERFORMANCE_MD;
    }
    /* NORMAL mode is allowed up to 1920 Hz (AN6119 Table 10; ST's
     * xl_setup() rejects it at 3840/7680). */
    return (sample_rate_hz <= 1920) ? LSM6DSV320X_XL_NORMAL_MD
                                    : LSM6DSV320X_XL_HIGH_PERFORMANCE_MD;
}

/* Gyro counterpart: LOW_POWER is only allowed below 480 Hz (AN6119
 * Table 13); above that the gyro runs high-performance. */
static lsm6dsv320x_gy_mode_t resolve_op_mode_g(bool high_performance,
                                               uint16_t sample_rate_hz)
{
    if (high_performance || sample_rate_hz >= 480) {
        return LSM6DSV320X_GY_HIGH_PERFORMANCE_MD;
    }
    return LSM6DSV320X_GY_LOW_POWER_MD;
}

/* ---- init --------------------------------------------------------------- */

/* One config write: on failure log which step, mark ERROR, fail init. */
#define CHIP_CFG(step, call) do {                                          \
        int32_t _rc = (call);                                              \
        if (_rc != 0) {                                                    \
            ESP_LOGE(TAG, "IMU%u init: %s failed (rc=%ld)",                \
                     (unsigned)imu_index, (step), (long)_rc);              \
            inst->per_state = IDL0_IMU_ERROR;                              \
            return false;                                                  \
        }                                                                  \
    } while (0)

bool imu_chip_init(stmdev_ctx_t *ctx, imu_chip_instance_t *inst,
                   uint8_t imu_index, bool is_spi_bus,
                   uint16_t sample_rate_hz, uint8_t accel_range_g,
                   uint16_t gyro_range_dps, bool high_performance,
                   bool hg_enabled, uint16_t hg_sample_rate_hz,
                   uint16_t hg_range_g)
{
    inst->per_state = IDL0_IMU_ABSENT;
    inst->probe = (idl0_imu_probe_t){ .status = IDL0_IMU_PROBE_NO_RESPONSE };

    /* WHO_AM_I first: nothing is written before the part is identified. */
    uint8_t who = 0;
    if (lsm6dsv320x_device_id_get(ctx, &who) != 0) {
        ESP_LOGW(TAG, "IMU%u WHO_AM_I read failed", (unsigned)imu_index);
        return false;
    }
    inst->probe.status   = IDL0_IMU_PROBE_ANSWERED;
    inst->probe.who_am_i = who;
    if (who != LSM6DSV320X_ID) {
        ESP_LOGW(TAG, "IMU%u WHO_AM_I=0x%02X, expected 0x%02X (LSM6DSV320X); "
                      "left unconfigured", (unsigned)imu_index, who, LSM6DSV320X_ID);
        return false;
    }

    lsm6dsv320x_data_rate_t odr;
    lsm6dsv320x_fifo_xl_batch_t xl_bdr;
    lsm6dsv320x_fifo_gy_batch_t gy_bdr;
    lsm6dsv320x_xl_full_scale_t xl_fs;
    lsm6dsv320x_gy_full_scale_t gy_fs;
    if (!odr_code(sample_rate_hz, &odr, &xl_bdr, &gy_bdr) ||
        !xl_fs_code(accel_range_g, &xl_fs) || !gy_fs_code(gyro_range_dps, &gy_fs)) {
        /* Config validation should have made this unreachable. */
        ESP_LOGE(TAG, "IMU%u init: off-list setting (%u Hz, %u g, %u dps) — not remapped",
                 (unsigned)imu_index, (unsigned)sample_rate_hz,
                 (unsigned)accel_range_g, (unsigned)gyro_range_dps);
        inst->per_state = IDL0_IMU_ERROR;
        return false;
    }

    if (hg_enabled) {
        /* High-g (CTRL1_XL_HG, XL_HG_BATCH_EN) is bring-up phase 3. */
        ESP_LOGW(TAG, "IMU%u: high-g requested (%u Hz, %u g) but not implemented "
                      "until phase 3; left off", (unsigned)imu_index,
                 (unsigned)hg_sample_rate_hz, (unsigned)hg_range_g);
        hg_enabled = false;
    }

    /* Software reset (ST's sequence powers the sensors down first and polls
     * SW_RESET until it clears). */
    CHIP_CFG("sw_reset", lsm6dsv320x_sw_reset(ctx));

    /* SPI instance: disable the I2C/I3C front end so SPI traffic to the SD
     * card on the same SCK/MOSI lines is never decoded as I2C. IF_CFG is not
     * cleared by SW_RESET, so it is written on every init; pods keep the
     * power-on default (I2C enabled). */
    if (is_spi_bus) {
        CHIP_CFG("ui_i2c_i3c_mode_set",
                 lsm6dsv320x_ui_i2c_i3c_mode_set(ctx, LSM6DSV320X_I2C_I3C_DISABLE));
    }

    /* BDU is 1 at reset on this part (0 on the DSO32); write it anyway. */
    CHIP_CFG("block_data_update_set", lsm6dsv320x_block_data_update_set(ctx, PROPERTY_ENABLE));

    CHIP_CFG("xl_full_scale_set", lsm6dsv320x_xl_full_scale_set(ctx, xl_fs));
    /* ST's setter read-modify-writes CTRL6 and parks the gyro while FS_G
     * changes, as the datasheet requires. */
    CHIP_CFG("gy_full_scale_set", lsm6dsv320x_gy_full_scale_set(ctx, gy_fs));

    const lsm6dsv320x_xl_mode_t xl_mode =
        imu_chip_resolve_op_mode_xl(high_performance, hg_enabled, sample_rate_hz);
    const lsm6dsv320x_gy_mode_t gy_mode = resolve_op_mode_g(high_performance, sample_rate_hz);
    if (!high_performance && (xl_mode == LSM6DSV320X_XL_HIGH_PERFORMANCE_MD ||
                              gy_mode == LSM6DSV320X_GY_HIGH_PERFORMANCE_MD)) {
        ESP_LOGI(TAG, "IMU%u: low-power mode not available at %u Hz for %s; "
                      "running high-performance", (unsigned)imu_index,
                 (unsigned)sample_rate_hz,
                 xl_mode == LSM6DSV320X_XL_HIGH_PERFORMANCE_MD ? "accel+gyro" : "gyro");
    }
    CHIP_CFG("xl_setup", lsm6dsv320x_xl_setup(ctx, odr, xl_mode));
    CHIP_CFG("gy_setup", lsm6dsv320x_gy_setup(ctx, odr, gy_mode));

    /* FIFO: batch both at the ODR, continuous (STREAM) mode. */
    CHIP_CFG("fifo_xl_batch_set", lsm6dsv320x_fifo_xl_batch_set(ctx, xl_bdr));
    CHIP_CFG("fifo_gy_batch_set", lsm6dsv320x_fifo_gy_batch_set(ctx, gy_bdr));
    CHIP_CFG("fifo_mode_set", lsm6dsv320x_fifo_mode_set(ctx, LSM6DSV320X_STREAM_MODE));

    inst->have_gyro     = false;
    inst->gyro_fresh    = false;
    inst->overrun_flag  = false;
    inst->reuse_count   = 0;
    inst->unknown_count = 0;
    inst->per_state     = IDL0_IMU_OK;
    ESP_LOGI(TAG, "IMU%u up (%s): ODR=%u Hz, accel ±%u g, gyro ±%u dps, %s",
             (unsigned)imu_index, is_spi_bus ? "SPI" : "I2C",
             (unsigned)sample_rate_hz, (unsigned)accel_range_g,
             (unsigned)gyro_range_dps,
             high_performance ? "high-performance" : "low-power where allowed");
    return true;
}

/* ---- FIFO --------------------------------------------------------------- */

uint16_t imu_chip_fifo_count(stmdev_ctx_t *ctx, imu_chip_instance_t *inst)
{
    lsm6dsv320x_fifo_status_t st = {0};
    if (lsm6dsv320x_fifo_status_get(ctx, &st) != 0) {
        inst->per_state = IDL0_IMU_ERROR;
        return 0;
    }
    if (st.fifo_ovr) inst->overrun_flag = true;
    return st.fifo_level;
}

/* One drain's raw FIFO words. Static (3.5 KB is too big for the IMU task
 * stack); the IMU task drains IMUs one at a time, so there is no
 * reentrancy. */
static uint8_t s_burst[IMU_CHIP_BURST_MAX_WORDS * 7];

size_t imu_chip_drain(stmdev_ctx_t *ctx, imu_chip_instance_t *inst,
                      idl0_imu_sample_t *out, size_t max_pairs,
                      int16_t out_hg[][3], size_t max_hg, size_t *n_hg)
{
    if (n_hg != NULL) *n_hg = 0;
    if (inst->per_state != IDL0_IMU_OK || max_pairs == 0) return 0;

    const uint16_t level = imu_chip_fifo_count(ctx, inst);
    if (inst->per_state != IDL0_IMU_OK || level == 0) return 0;

    size_t words = level;
    if (words > IMU_CHIP_BURST_MAX_WORDS) words = IMU_CHIP_BURST_MAX_WORDS;
    /* T30: cap to max_pairs BEFORE the read. The parse emits at most one
     * pair per word, and the burst physically dequeues every word it reads,
     * so reading more than max_pairs words would drop the excess. The tail
     * stays in the chip FIFO for the next poll instead. */
    if (words > max_pairs) words = max_pairs;

    /* One transaction from FIFO_DATA_OUT_TAG (0x78): with IF_INC=1 (reset
     * default) the address runs 0x78..0x7E and rolls back to 0x78 for the
     * next word, streaming consecutive 7-byte words [tag, XL, XH, YL, YH,
     * ZL, ZH]. */
    if (lsm6dsv320x_read_reg(ctx, LSM6DSV320X_FIFO_DATA_OUT_TAG, s_burst,
                             (uint16_t)(words * 7)) != 0) {
        inst->per_state = IDL0_IMU_ERROR;
        return 0;
    }

    size_t n_pairs = 0;
    for (size_t w = 0; w < words && n_pairs < max_pairs; w++) {
        const uint8_t *word = &s_burst[w * 7];
        const uint8_t tag = word[0] >> 3;
        const uint8_t *data = &word[1];

        if (tag == TAG_GY_NC) {
            memcpy(inst->pending_gyro, data, sizeof(inst->pending_gyro));
            inst->have_gyro  = true;
            inst->gyro_fresh = true;
        } else if (tag == TAG_XL_NC) {
            /* One sample per accel, paired with the most recent gyro. A lone
             * accel on a FIFO phase slip reuses the last gyro (<= 1 sample
             * stale) rather than being dropped; only a never-seen gyro
             * (startup) skips. */
            if (!inst->have_gyro) continue;
            if (!inst->gyro_fresh) inst->reuse_count++;
            inst->gyro_fresh = false;
            memcpy(out[n_pairs].gyro,  inst->pending_gyro, sizeof(out[n_pairs].gyro));
            memcpy(out[n_pairs].accel, data, sizeof(out[n_pairs].accel));
            n_pairs++;
        } else if (tag == TAG_XL_HG && out_hg != NULL && n_hg != NULL) {
            /* Self-contained X/Y/Z triplet; never touches the pair builder. */
            if (*n_hg < max_hg) {
                memcpy(out_hg[*n_hg], data, 6);
                (*n_hg)++;
            }
        } else {
            inst->unknown_count++;
        }
    }
    return n_pairs;
}

bool imu_chip_check_overrun(imu_chip_instance_t *inst)
{
    const bool was = inst->overrun_flag;
    inst->overrun_flag = false;
    return was;
}

void imu_chip_take_pair_diag(imu_chip_instance_t *inst,
                             uint32_t *reuse, uint32_t *unknown)
{
    if (reuse)   *reuse   = inst->reuse_count;
    if (unknown) *unknown = inst->unknown_count;
    inst->reuse_count   = 0;
    inst->unknown_count = 0;
}
