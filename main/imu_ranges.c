/* See imu_ranges.h. Pure C99 — no ESP-IDF includes. */

#include "imu_ranges.h"

#include <stddef.h>

/* Low-g ODR ladder, Hz (CTRL1/CTRL2 ODR codes 0011..1100). */
static const int32_t ODR_SET[] = {
    15, 30, 60, 120, 240, 480, 960, 1920, 3840, 7680,
};

/* Low-g full-scale (g) and typical sensitivity (mg/LSB), datasheet
 * mechanical characteristics table. */
static const struct { int32_t g; float mg_per_lsb; } ACCEL[] = {
    {  2, 0.061f },
    {  4, 0.122f },
    {  8, 0.244f },
    { 16, 0.488f },
};

/* Gyro full-scale (dps) and typical sensitivity (mdps/LSB). */
static const struct { int32_t dps; float mdps_per_lsb; } GYRO[] = {
    {  250,   8.75f },
    {  500,  17.5f  },
    { 1000,  35.0f  },
    { 2000,  70.0f  },
    { 4000, 140.0f  },
};

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

bool idl1_imu_odr_valid(int32_t hz)
{
    for (size_t i = 0; i < COUNT(ODR_SET); i++) {
        if (ODR_SET[i] == hz) return true;
    }
    return false;
}

bool idl1_imu_accel_range_valid(int32_t g)
{
    return idl1_imu_accel_g_per_lsb(g) > 0.0f;
}

bool idl1_imu_gyro_range_valid(int32_t dps)
{
    return idl1_imu_gyro_dps_per_lsb(dps) > 0.0f;
}

float idl1_imu_accel_g_per_lsb(int32_t g)
{
    for (size_t i = 0; i < COUNT(ACCEL); i++) {
        if (ACCEL[i].g == g) return ACCEL[i].mg_per_lsb / 1000.0f;
    }
    return 0.0f;
}

float idl1_imu_gyro_dps_per_lsb(int32_t dps)
{
    for (size_t i = 0; i < COUNT(GYRO); i++) {
        if (GYRO[i].dps == dps) return GYRO[i].mdps_per_lsb / 1000.0f;
    }
    return 0.0f;
}
