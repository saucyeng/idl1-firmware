/* LSM6DSV320X IMUs — shared types and the SPI (onboard IMU0) API.
 *
 * IDL1 carries three LSM6DSV320X: the onboard IMU0 on SPI2_HOST (this API,
 * imu_driver.c; the bus is initialised by sd_logger) and the front/rear pods
 * on I2C (imu_driver_i2c.h). Register-level work for both is in
 * imu_chip_dsv320x.c, on top of the vendored ST driver
 * (components/lsm6dsv320x_STdC, v5.4.0).
 *
 * Spec contracts: §3 (hardware), §4 (firmware), §5.4 / §5.5 (IMU
 * channel mask + IMU_SAMPLE record), §8 (per-IMU ODR / range / mode).
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/spi_master.h"

/* Number of physical IMU slots. Matches §3 (IMU0 onboard sprung mass,
 * IMU1 front unsprung, IMU2 rear unsprung) and the channel-mask layout
 * in §5.4 (bits 0-17 = 3 × 6 axes). */
#define IDL0_IMU_COUNT 3

/* IMU subsystem state, mapped 1:1 onto the §7.3 IMU: status token.
 *
 * Aggregate across all configured IMUs:
 *   ABSENT  — no IMU was even attempted (init never called)
 *   OK      — every attempted IMU answered WHO_AM_I and configured cleanly
 *   PARTIAL — some attempted IMUs are up, others failed (e.g. unpopulated)
 *   ERROR   — at least one IMU had a read/write failure after init
 */
typedef enum {
    IDL0_IMU_ABSENT = 0,
    IDL0_IMU_OK,
    IDL0_IMU_PARTIAL,
    IDL0_IMU_ERROR,
} idl0_imu_state_t;

/* WHO_AM_I (0x0F) of the LSM6DSV320X fitted at every IDL1 IMU position. */
#define IDL1_IMU_WHO_AM_I_DSV320X 0x73

/* Outcome of an IMU's WHO_AM_I read at init, per IMU index. */
typedef enum {
    IDL0_IMU_PROBE_NOT_RUN = 0,  /* init never attempted for this index */
    IDL0_IMU_PROBE_NO_RESPONSE,  /* bus error / address NACK on the read */
    IDL0_IMU_PROBE_ANSWERED,     /* read completed; value in who_am_i */
} idl0_imu_probe_status_t;

typedef struct {
    idl0_imu_probe_status_t status;
    uint8_t who_am_i;  /* raw WHO_AM_I byte; valid only when ANSWERED */
} idl0_imu_probe_t;

/* One drained low-g/gyro sample, ready to be timestamped + framed by
 * imu_task. High-g (pods, phase 3) is not a field here: it lands as
 * separate CHANNEL_SAMPLE records on registry channels 24-29. */
typedef struct {
    int16_t accel[3];  /* X, Y, Z — raw int16 LSB, no scaling */
    int16_t gyro[3];   /* X, Y, Z — raw int16 LSB */
} idl0_imu_sample_t;

/* Aggregate state — see enum doc above.
 * T32: this aggregates only the SPI driver's IMUs (IMU0). With IMU1/IMU2 on
 * I²C (imu_driver_i2c.c) it is blind to their state — use
 * idl0_imu_task_state() (imu_task.h) for the whole-device aggregate that
 * status.c reports. Kept here for the SPI driver's own bookkeeping/tests. */
idl0_imu_state_t idl0_imu_state(void);
const char *idl0_imu_state_str(idl0_imu_state_t state);

/* Returns true if the given IMU initialised successfully and is currently
 * usable. False for unpopulated slots, init failures, or post-init errors.
 * Bounds-checks imu_index. */
bool idl0_imu_is_active(uint8_t imu_index);

/* Raw per-instance state (ABSENT/OK/ERROR — PARTIAL is aggregate-only,
 * never assigned per-IMU). Lets a caller distinguish a never-populated slot
 * (ABSENT) from one that came up and later faulted (ERROR), which
 * idl0_imu_is_active() alone cannot. Bounds-checks imu_index (ABSENT if out
 * of range). */
idl0_imu_state_t idl0_imu_per_state(uint8_t imu_index);

/* Initialise one IMU (by index 0..IDL0_IMU_COUNT-1) on the shared SPI
 * host: add the device with its CS pin (per pins.h), then imu_chip_init():
 * WHO_AM_I (0x73) before any register write, configure accel + gyro at the
 * given ODR / range / mode, FIFO STREAM with BDR equal to ODR. On IDL1 only
 * IMU0 has a chip-select; indices 1/2 (I2C pods) are refused.
 *
 * `sample_rate_hz`: 15 / 30 / 60 / 120 / 240 / 480 / 960 / 1920 / 3840 / 7680.
 * `accel_range_g`:  2, 4, 8, or 16.
 * `gyro_range_dps`: 250, 500, 1000, 2000, or 4000.
 * `high_performance`: true -> high-performance mode on accel and gyro;
 *                     false -> the low-power modes the ODR allows.
 * Off-list values are rejected at config-validation time, not mapped to the
 * nearest supported value — a documented contract (decisions.md P2/P3);
 * one that still reaches here fails init.
 *
 * Returns true on success (this IMU's per-instance state -> OK).
 * On failure leaves the per-instance state at ABSENT (chip absent /
 * miswired / WHO_AM_I wrong) or ERROR (a configuration write failed).
 * Independent of other IMUs — failure here does not affect siblings. */
bool idl0_imu_init_one(spi_host_device_t spi_host,
                       uint8_t imu_index,
                       uint16_t sample_rate_hz,
                       uint8_t accel_range_g,
                       uint16_t gyro_range_dps,
                       bool high_performance);

/* WHO_AM_I probe result of the last idl0_imu_init_one() for this index.
 * NOT_RUN for out-of-range or never-initialised indices. */
idl0_imu_probe_t idl0_imu_probe_result(uint8_t imu_index);

/* Per-IMU FIFO_STATUS unread-words count (0..512). Returns 0 if this
 * IMU is not active. */
uint16_t idl0_imu_fifo_count(uint8_t imu_index);

/* Burst-drain up to `max_pairs` (gyro, accel) pairs in a single DMA SPI
 * transaction (the FIFO streams from 0x78 with IF_INC auto-increment). The
 * SPI-bus analogue of idl0_imu_drain_i2c(): one transaction replaces the
 * per-word tag+data reads, removing the per-transaction overhead that made the
 * SPI drain dominate the poll cycle. Latches the overrun flag and carries the
 * pending gyro across calls. Returns the number of pairs written to `out`. */
size_t idl0_imu_drain(uint8_t imu_index, idl0_imu_sample_t *out, size_t max_pairs);

/* Was this IMU's FIFO overrun bit set since the last call? Clears on
 * read. Used by imu_task to log overruns (§5.5: not written to file). */
bool idl0_imu_check_overrun(uint8_t imu_index);

/* Read + clear this IMU's pair-builder diagnostics since the last call:
 * `reuse` = accels that reused a stale gyro on a FIFO phase slip (the samples
 * the lossless pairing recovers); `unknown` = unrecognised FIFO tags (a
 * corruption signal, 0 on a clean bus). Either out-pointer may be NULL. */
void idl0_imu_take_pair_diag(uint8_t imu_index, uint32_t *reuse, uint32_t *unknown);
