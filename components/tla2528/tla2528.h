/* TI TLA2528 8-channel 12-bit ADC / GPIO expander — IDL1 board driver.
 *
 * Bring-up phase 1 scope: probe one address, configure the two board LED
 * channels as push-pull outputs, drive the LEDs. No ADC sequencer yet
 * (phase 3). Register map and I2C opcodes:
 * docs/program/findings/2026-08-31-pod-delta/tla2528-regmap.md, with
 * decisions.md P5 overriding it (no autonomous mode, no RECENT_CHx, nothing
 * above 0x12).
 *
 * The caller owns the I2C bus and the address (the component does not
 * depend on main/pins.h). Every function is safe to call when the chip never
 * answered: boot must survive a silent TLA2528 (decisions.md H2).
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"

/* SCL clock for this device, Hz. Matches the shared IDL1 bus rate
 * (main/i2c_bus.h IDL1_I2C_SCL_HZ); the part itself allows up to 1 MHz
 * (3.4 MHz HS), the ESP32-C6 controller does not. */
#define TLA2528_SCL_HZ 400000

/* IDL1 board wiring of the TLA2528 channels this driver touches. Plain
 * TLA2528 channel numbers (0..7), NOT ESP32 GPIOs. They mirror
 * IDL1_TLA_CH_LED_RED/_GREEN and IDL1_TLA_CH_ANA_R/_F in main/pins.h;
 * main.c static-asserts that the two stay equal. */
#define TLA2528_CH_LED_RED    6   /* D1, active-high GPO — recording (H1) */
#define TLA2528_CH_LED_GREEN  7   /* D2, active-high GPO — power + wireless (H1) */
#define TLA2528_CH_ANA_R      4   /* AIN4, rear pod analog — stays analog */
#define TLA2528_CH_ANA_F      5   /* AIN5, front pod analog — stays analog */

/* SYSTEM_STATUS (0x00) identity check (the part has no device-ID register;
 * bring-up correction R8). Bit 7 RSVD "must read 1b"; bits 4 and 1 are
 * reserved and read 0b. The other bits (SEQ_STATUS, I2C_SPEED, OSR_DONE,
 * CRC_ERR_FUSE, BOR) are live status, so they are masked out. Reset value
 * 0x80; BOR (bit 0) is normally set after power-up, giving 0x81. */
#define TLA2528_SYSTEM_STATUS_ID_MASK   0x92u
#define TLA2528_SYSTEM_STATUS_ID_VALUE  0x80u

/* Result of the boot probe, for the self-test report. */
typedef struct {
    bool    probed;         /* tla2528_init() has run at least once */
    uint8_t addr;           /* 7-bit I2C address probed */
    bool    acked;          /* the SYSTEM_STATUS read completed (device ACKed) */
    uint8_t system_status;  /* raw SYSTEM_STATUS byte; valid only if acked */
    bool    status_id_ok;   /* (system_status & ID_MASK) == ID_VALUE */
    bool    configured;     /* LED channels configured; == tla2528_is_up() */
} tla2528_probe_t;

/* Probe the TLA2528 at exactly `addr` (7-bit) on `bus` — no scanning
 * (decisions.md H2) — and, if it answers, configure channels 6/7 as
 * push-pull digital outputs with both LEDs off. AIN4/AIN5 are left analog.
 *
 * Probe = one SYSTEM_STATUS single-register read. On NACK/timeout: one
 * loud ESP_LOGE naming the address, the device is released from the bus,
 * returns false. A SYSTEM_STATUS value that fails the ID mask check is
 * logged as a warning but is not a failure. Returns true once the LED
 * channels are configured. Call again to retry (idempotent w.r.t. the
 * device handle). */
bool tla2528_init(i2c_master_bus_handle_t bus, uint8_t addr);

/* True once tla2528_init() has probed and configured the chip. */
bool tla2528_is_up(void);

/* Drive the two board LEDs (true = lit). No-op if !tla2528_is_up(). An I2C
 * write failure is logged at warning level and otherwise ignored — the
 * LEDs are indication only. */
/* Returns false when the write failed (bus busy or the chip didn't answer),
 * so a caller can retry. */
bool tla2528_led_set(bool red_on, bool green_on);

/* Copy the boot-probe result into *out (zeroed if init never ran). */
void tla2528_get_probe(tla2528_probe_t *out);
