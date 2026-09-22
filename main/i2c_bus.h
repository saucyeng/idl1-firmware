/* Shared I2C bus owner (IDL1).
 *
 * One bus, one owner: SDA/SCL from pins.h carry the TLA2528, both pod IMUs
 * and the Qwiic port. Every driver on that bus takes its handle from here
 * and adds its own device; nobody else calls i2c_new_master_bus().
 */

#pragma once

#include "driver/i2c_master.h"

/* Per-device SCL clock every driver on this bus uses, Hz. 400 kHz: the
 * ESP32-C6 I2C controller lists only 100/400 kbit/s, and the pods hang on
 * cables with 4k7 pull-ups (decisions.md P2). */
#define IDL1_I2C_SCL_HZ 400000

/* Lazily creates the shared I2C bus (I2C_NUM_0, SDA/SCL from pins.h,
 * internal pull-ups disabled -- the board carries 4k7 externals) on first
 * call; returns the same handle on every subsequent call. Returns NULL if
 * i2c_new_master_bus() fails (logged once per failed attempt). Not
 * thread-safe on the first call: call it from app_main's boot sequence
 * before any task that uses the bus starts. */
i2c_master_bus_handle_t idl1_i2c_bus_get(void);
