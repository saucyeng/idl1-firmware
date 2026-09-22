/* Host-compiled unit test for imu_ranges.c (LSM6DSV320X value sets and
 * sensitivities — decisions.md P2/P3, corrections R6/R11).
 *
 * Build & run (from test/):
 *   cc -std=c99 -I../main test_imu_ranges.c ../main/imu_ranges.c -o test_imu_ranges
 *   ./test_imu_ranges
 */

#include <math.h>
#include <stdio.h>

#include "imu_ranges.h"

static int g_checks = 0;
#define CHECK(cond) do { g_checks++; if (!(cond)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); return 1; } } while (0)

static int near(float a, float b) { return fabsf(a - b) <= 1e-9f + 1e-6f * fabsf(b); }

static int test_odr(void) {
    static const int ok[] = { 15, 30, 60, 120, 240, 480, 960, 1920, 3840, 7680 };
    for (unsigned i = 0; i < sizeof(ok) / sizeof(ok[0]); i++) CHECK(idl1_imu_odr_valid(ok[i]));
    /* Old DSO32 values and near-misses are rejected, not remapped. */
    static const int bad[] = { 0, 7, 8, 12, 13, 26, 52, 104, 208, 416, 833, 1666, 481, -480, 7681 };
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) CHECK(!idl1_imu_odr_valid(bad[i]));
    CHECK(idl1_imu_odr_valid(IDL1_IMU_DEFAULT_ODR_HZ));
    CHECK(IDL1_IMU_DEFAULT_ODR_HZ == 480);
    return 0;
}

static int test_accel(void) {
    CHECK(idl1_imu_accel_range_valid(2));
    CHECK(idl1_imu_accel_range_valid(16));
    CHECK(!idl1_imu_accel_range_valid(32));   /* DSO32-only; high-g path now */
    CHECK(!idl1_imu_accel_range_valid(0));
    CHECK(near(idl1_imu_accel_g_per_lsb(2),  0.000061f));
    CHECK(near(idl1_imu_accel_g_per_lsb(4),  0.000122f));
    CHECK(near(idl1_imu_accel_g_per_lsb(8),  0.000244f));
    CHECK(near(idl1_imu_accel_g_per_lsb(16), 0.000488f));
    CHECK(idl1_imu_accel_g_per_lsb(32) == 0.0f);
    CHECK(idl1_imu_accel_range_valid(IDL1_IMU_DEFAULT_ACCEL_RANGE_G));
    return 0;
}

static int test_gyro(void) {
    CHECK(!idl1_imu_gyro_range_valid(125));   /* gone on the DSV320X */
    CHECK(idl1_imu_gyro_range_valid(4000));
    CHECK(near(idl1_imu_gyro_dps_per_lsb(250),  0.00875f));
    CHECK(near(idl1_imu_gyro_dps_per_lsb(500),  0.0175f));
    CHECK(near(idl1_imu_gyro_dps_per_lsb(1000), 0.035f));
    CHECK(near(idl1_imu_gyro_dps_per_lsb(2000), 0.070f));
    CHECK(near(idl1_imu_gyro_dps_per_lsb(4000), 0.140f));
    CHECK(idl1_imu_gyro_dps_per_lsb(125) == 0.0f);
    /* The point of R11: the datasheet sensitivity is not range/32768. */
    CHECK(!near(idl1_imu_gyro_dps_per_lsb(2000), 2000.0f / 32768.0f));
    CHECK(idl1_imu_gyro_range_valid(IDL1_IMU_DEFAULT_GYRO_RANGE_DPS));
    return 0;
}

int main(void)
{
    if (test_odr())   return 1;
    if (test_accel()) return 1;
    if (test_gyro())  return 1;
    printf("PASS — %d checks\n", g_checks);
    return 0;
}
