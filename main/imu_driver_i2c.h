/* LSM6DSV320X pod IMUs on the shared IDL1 I2C bus.
 *
 * IDL1 runs one IMU type over two buses by design: the onboard IMU0 on SPI
 * (imu_driver.c, shared with the SD card) and the front/rear pods on I2C
 * (this file, shared with the TLA2528), at the JP1-strapped addresses in
 * pins.h. Both adapters drive the chip through imu_chip_dsv320x.c;
 * imu_task.c routes each IMU index to its bus via s_imu_bus[].
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "imu_driver.h"   /* for idl0_imu_sample_t and IDL0_IMU_COUNT */

/* Initialise one pod IMU at the given 7-bit address on the shared bus from
 * idl1_i2c_bus_get() (i2c_bus.h), at IDL1_I2C_SCL_HZ. Probes the address
 * first (an absent pod is logged at INFO — a normal configuration), then
 * runs imu_chip_init(): WHO_AM_I (0x73) before any register write, then
 * accel + gyro at the requested ODR / range / mode and FIFO STREAM with BDR
 * equal to ODR. Value sets as idl0_imu_init_one() (imu_driver.h).
 *
 * Returns true on success. On failure this instance stays inactive so the
 * other IMUs are unaffected. */
bool idl0_imu_init_one_i2c(uint8_t imu_index,
                           uint8_t i2c_addr,
                           uint16_t sample_rate_hz,
                           uint8_t accel_range_g,
                           uint16_t gyro_range_dps,
                           bool high_performance);

/* Per-IMU runtime queries — mirror the SPI versions in imu_driver.h but
 * operate on the I2C adapter's state. */
bool     idl0_imu_is_active_i2c(uint8_t imu_index);
/* Was idl0_imu_init_one_i2c() ever called for this index, regardless of
 * outcome? Lets a caller (idl0_imu_task_state(), idl0_imu_task_reinit_failed()
 * in imu_task.c) tell "never configured" from "configured but currently
 * down". Bounds-checks imu_index (false if out of range). */
bool     idl0_imu_i2c_init_attempted(uint8_t imu_index);
/* WHO_AM_I probe result of the last idl0_imu_init_one_i2c() for this index
 * (NO_RESPONSE covers an absent pod). NOT_RUN if never initialised. */
idl0_imu_probe_t idl0_imu_probe_result_i2c(uint8_t imu_index);
uint16_t idl0_imu_fifo_count_i2c(uint8_t imu_index);
bool     idl0_imu_check_overrun_i2c(uint8_t imu_index);
/* Read + clear the pair-builder diagnostics — see idl0_imu_take_pair_diag()
 * in imu_driver.h. Either out-pointer may be NULL. */
void     idl0_imu_take_pair_diag_i2c(uint8_t imu_index, uint32_t *reuse, uint32_t *unknown);

/* Burst-drain up to `max_pairs` (gyro, accel) pairs in one I2C transaction
 * (FIFO words stream from 0x78 with auto-increment) — see imu_chip_drain().
 * Returns the number of pairs written to `out`. */
size_t idl0_imu_drain_i2c(uint8_t imu_index, idl0_imu_sample_t *out,
                          size_t max_pairs);
