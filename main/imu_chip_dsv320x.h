/* LSM6DSV320X chip operations, shared by both bus adapters.
 *
 * IDL1 runs one IMU type over two buses by design: the onboard IMU0 on the
 * SPI bus it shares with the SD card (imu_driver.c), the two pods on the
 * I2C bus they share with the TLA2528 (imu_driver_i2c.c). Everything that
 * talks to the chip through registers lives here, parameterised only by the
 * ST `stmdev_ctx_t` the adapter builds; the adapters keep only device-add,
 * the platform read/write/delay glue and their public idl0_imu_* wrappers.
 *
 * ST driver: components/lsm6dsv320x_STdC (v5.4.0). Symbols used, checked
 * against lsm6dsv320x_reg.h (pod-firmware-plan.md PT3):
 *   lsm6dsv320x_device_id_get, _sw_reset, _ui_i2c_i3c_mode_set,
 *   _block_data_update_set, _xl_full_scale_set (lsm6dsv320x_xl_full_scale_t),
 *   _gy_full_scale_set (lsm6dsv320x_gy_full_scale_t), _xl_setup / _gy_setup
 *   (lsm6dsv320x_data_rate_t + _xl_mode_t / _gy_mode_t; the older
 *   _xl_data_rate_set / _xl_mode_set are deprecated upstream),
 *   _fifo_xl_batch_set / _fifo_gy_batch_set, _fifo_mode_set (STREAM_MODE),
 *   _fifo_status_get (level + overrun), _read_reg (FIFO burst from 0x78).
 *   High-g (phase 3): _hg_xl_data_rate_set, _hg_xl_full_scale_set,
 *   _fifo_hg_xl_batch_set; FIFO tag LSM6DSV320X_XL_HG_TAG = 0x1D.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "imu_driver.h"         /* idl0_imu_state_t, idl0_imu_sample_t, probe types */
#include "lsm6dsv320x_reg.h"

/* Largest FIFO burst one drain reads, words (7 bytes each: tag + 6 data).
 * The DSV320X FIFO_STATUS level field is 9 bits, so it never reports more
 * than 511 unread words; 512 covers a full FIFO in one burst. */
#define IMU_CHIP_BURST_MAX_WORDS 512

/* Per-IMU chip state. Owned by the adapter (one per IMU index); only the
 * functions below touch it. */
typedef struct {
    idl0_imu_state_t per_state;    /* ABSENT / OK / ERROR */
    idl0_imu_probe_t probe;        /* WHO_AM_I read at the last init */
    bool     overrun_flag;         /* FIFO overrun seen since last check */
    int16_t  pending_gyro[3];      /* last gyro word, raw LSB */
    bool     have_gyro;
    bool     gyro_fresh;
    uint32_t reuse_count;          /* accels paired with a stale gyro, count */
    uint32_t unknown_count;        /* unrecognised FIFO tags, count */
    int8_t   freq_fine;            /* INTERNAL_FREQ_FINE at init (F12) */
} imu_chip_instance_t;

/* Identify and configure one LSM6DSV320X. WHO_AM_I is read before any
 * register write; anything but 0x73 leaves the chip untouched and the state
 * at ABSENT. Then: software reset, I2C/I3C disabled on the SPI instance
 * (`is_spi_bus`; IF_CFG survives SW_RESET so it is written every init), BDU,
 * low-g and gyro full scale, ODR + operating mode, FIFO batching at the ODR
 * and STREAM mode.
 *
 * sample_rate_hz / accel_range_g / gyro_range_dps must already be on the
 * valid lists in imu_ranges.h (config validation rejects off-list values
 * before they get here); an off-list value fails init instead of being
 * remapped. high_performance selects the operating mode (see
 * imu_chip_resolve_op_mode_xl()). hg_enabled / hg_sample_rate_hz (Hz) /
 * hg_range_g (g) enable the high-g accelerometer into the FIFO (F11). The
 * timestamp counter runs and a timestamp word is batched every 32 samples
 * (F12); FREQ_FINE is read into the instance.
 *
 * Returns true with per_state = OK; false with ABSENT (not a DSV320X /
 * no answer) or ERROR (a configuration write failed). */
bool imu_chip_init(stmdev_ctx_t *ctx, imu_chip_instance_t *inst,
                   uint8_t imu_index, bool is_spi_bus,
                   uint16_t sample_rate_hz, uint8_t accel_range_g,
                   uint16_t gyro_range_dps, bool high_performance,
                   bool hg_enabled, uint16_t hg_sample_rate_hz,
                   uint16_t hg_range_g);

/* The OP_MODE_XL value to write for the requested mode (decisions.md P1
 * interlock): with high-g enabled the low-g accelerometer must be in
 * high-performance mode, so a low-power request is overridden. Without
 * high-g, high_performance=false selects NORMAL where the datasheet allows
 * it at `sample_rate_hz` (<= 1920 Hz) and high-performance above. */
lsm6dsv320x_xl_mode_t imu_chip_resolve_op_mode_xl(bool high_performance,
                                                  bool hg_enabled,
                                                  uint16_t sample_rate_hz);

/* Unread FIFO words (0..511), latching the overrun flag. Marks the
 * instance ERROR and returns 0 on a bus failure. */
uint16_t imu_chip_fifo_count(stmdev_ctx_t *ctx, imu_chip_instance_t *inst);

/* Burst-drain the FIFO: one bus transaction for up to min(level,
 * IMU_CHIP_BURST_MAX_WORDS, max_pairs) words, then pair gyro -> accel words
 * in memory. Returns the (gyro, accel) pairs written to `out`. High-g
 * triplets (tag 0x1D) and timestamp words (tag 0x04) go to `extra` (either
 * buffer may be NULL), the words with their position in the drain; neither
 * touches the pair builder.
 * Marks the instance ERROR and returns 0 on a bus failure. Single caller
 * (the IMU task): uses one static burst buffer. */
size_t imu_chip_drain(stmdev_ctx_t *ctx, imu_chip_instance_t *inst,
                      idl0_imu_sample_t *out, size_t max_pairs,
                      idl1_imu_drain_extra_t *extra);

/* The chip's TIMESTAMP register. Marks the instance ERROR and returns false
 * on a bus failure. */
bool imu_chip_read_timestamp(stmdev_ctx_t *ctx, imu_chip_instance_t *inst,
                             uint32_t *chip_ts);

/* Was the FIFO overrun flag latched since the last call? Clears it. */
bool imu_chip_check_overrun(imu_chip_instance_t *inst);

/* Read + clear the pair-builder diagnostics. Either pointer may be NULL. */
void imu_chip_take_pair_diag(imu_chip_instance_t *inst,
                             uint32_t *reuse, uint32_t *unknown);
