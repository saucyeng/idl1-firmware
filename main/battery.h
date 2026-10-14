/* Battery sense (BAT_SENSE, pins.h) — one averaged, calibrated reading of
 * the cell through the 2:3 divider. Shared by the boot self-test
 * (bringup.c) and the 1 Hz status line (status.c); the ADC unit and its
 * calibration are created once, on first use, and kept.
 *
 * There is no charger status pin on this board revision, so a charger
 * holding the cell node at its float voltage with no cell fitted reads the
 * same as a full cell. Telling those apart needs hardware (a charger STAT
 * line) — the reading is reported as volts, never as "present".
 */

#pragma once

#include <stdbool.h>

/* Divider (pins.h): R2 = 1M from BAT+, R3 = 2M to GND, so Vbat = Vpin * 3 / 2. */
#define IDL1_BAT_DIV_NUM 3
#define IDL1_BAT_DIV_DEN 2

/* Reads the battery. On success fills *raw_avg (averaged ADC count),
 * *pin_mv (calibrated pin voltage, mV) and *vbat_mv (cell voltage, mV) and
 * returns true; on failure sets *why and returns false. Thread-safe. */
bool idl1_battery_read(int *raw_avg, int *pin_mv, int *vbat_mv, const char **why);

/* Rough state of charge for a single Li-ion cell at rest, percent 0-100,
 * from `vbat_mv` by a piecewise-linear discharge curve. Only for the
 * deprecated "Battery: N%" status line and /ping "battery" key, kept one
 * revision for older apps (§7.3); the app does its own scaling from
 * BatteryMv. Reads 100 for a charger holding the node at float voltage. */
int idl1_battery_pct_estimate(int vbat_mv);
