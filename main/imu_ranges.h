/* LSM6DSV320X low-g / gyro value sets and sensitivities — pure C99,
 * host-testable (test/test_imu_ranges.c).
 *
 * The valid sets are decisions.md P2/P3 with bring-up correction R6 (7.5 Hz
 * is not representable in the integer config field, so the ODR set starts at
 * 15). Off-list config values are rejected per field, never remapped to a
 * nearest neighbour (P2/P3). Sensitivities are the datasheet's typical
 * values per full-scale, not range/32768 (correction R11).
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Shipped defaults. All three are runtime config values (imu.sample_rate_hz,
 * imu.accel_range_g, imu.gyro_range_dps), not compile-time settings. 480 Hz
 * is the bus-safe shared rate for both pods on one 400 kHz I2C bus (R5). */
#define IDL1_IMU_DEFAULT_ODR_HZ        480
#define IDL1_IMU_DEFAULT_ACCEL_RANGE_G  16
#define IDL1_IMU_DEFAULT_GYRO_RANGE_DPS 2000

/* True if `hz` is a valid imu.sample_rate_hz, Hz:
 * {15, 30, 60, 120, 240, 480, 960, 1920, 3840, 7680}. */
bool idl1_imu_odr_valid(int32_t hz);

/* True if `g` is a valid low-g imu.accel_range_g, g: {2, 4, 8, 16}. */
bool idl1_imu_accel_range_valid(int32_t g);

/* True if `dps` is a valid imu.gyro_range_dps, dps:
 * {250, 500, 1000, 2000, 4000}. */
bool idl1_imu_gyro_range_valid(int32_t dps);

/* Low-g accelerometer sensitivity at full-scale `g`, in g per LSB
 * (0.061 / 0.122 / 0.244 / 0.488 mg/LSB). 0.0f for an off-list range. */
float idl1_imu_accel_g_per_lsb(int32_t g);

/* Gyroscope sensitivity at full-scale `dps`, in dps per LSB
 * (8.75 / 17.5 / 35 / 70 / 140 mdps/LSB). 0.0f for an off-list range. */
float idl1_imu_gyro_dps_per_lsb(int32_t dps);
