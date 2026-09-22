# Spec: `pins_idl1.h` and Kconfig board selection

Status: **ready to transcribe at fork time.** Nothing here is installed today — `idl1-firmware`
does not exist and `idl0-firmware` is not edited by this spec.

- Authority: `idl1-program/decisions.md` — H1 (LEDs), H2 (TLA2528 address), H4 (header shape),
  P7·H3 (pod addresses). Where this spec's sources disagreed with decisions.md, decisions.md won.
- Pin assignments: `idl1-program/findings/2026-08-31-hw-delta/pins-delta.md` (netlist-derived).
  Not the IDL1 README — its five-change list is known incomplete.
- Written 2026-09-02.

## 1. Corrections to `pins-delta.md` PROVISIONALs (settled by decisions.md)

`pins-delta.md` itself has not been edited; these corrections are applied in this spec, which
supersedes the skeleton in its Deliverable 4.

| pins-delta.md said | decisions.md ruling | Applied here |
|---|---|---|
| `IDL1_TLA2528_ADDR 0x11` PROVISIONAL; recommend an I2C bus scan 0x10–0x17 at bring-up (lines 17, 257, 471) | **H2 FINAL**: expected address **0x10** (the documented R1=DNP/R2=DNP row, what the board becomes once NT1 is opened). **No scan, no sweep** — probe exactly one address, confirm with a device-ID read, diag-log a loud distinct failure on NACK. Boot must survive the ADC not answering (both board LEDs hang off it; only the GPIO15 onboard LED works in that state). NT1 is a copper net tie — opening it is a knife job, so the constant stays trivially editable while the owner resolves the board in hand. | `IDL1_I2C_ADDR_TLA 0x10`, single named constant, §3 |
| Pod IMU address "depends on JP1's populated state, not visible in a netlist" — PROVISIONAL (line 483) | **P7·H3 FINAL**: front pod = JP1 open = **0x6A** = spec IMU1; rear pod = JP1 closed = **0x6B** = spec IMU2. Firmware defines both, confirms each with WHO_AM_I at boot, diag-logs an absent pod rather than guessing. | `IDL1_I2C_ADDR_IMU_FRONT/_REAR`, §3 |
| LED assignment left open (questions.md Q1 proceeded on "D2 = status/heartbeat, D1 = fault") | **H1 FINAL**: **green** (D2, ch 7) = power and wireless connection; **red** (D1, ch 6) = recording. Blink patterns are the owner's call, not blocking. GPIO15 onboard LED kept as `IDL1_PIN_LED_ONBOARD`. | Channel macros named by color, roles in comments, §3 |
| Skeleton prefix `IDL1_TLA2528_CH_*` | **H4 FINAL** spells it `IDL1_TLA_CH_*` | Renamed throughout |

## 2. Design

Per H4: `IDL1_PIN_*` for real GPIOs typed `gpio_num_t`; `IDL1_TLA_CH_*` for TLA2528 channels as
plain ints; Kconfig choice `CONFIG_IDL_BOARD_IDL0` / `CONFIG_IDL_BOARD_IDL1`; `pins.h` reduced to
a thin `#if` including exactly one board header; drivers include `pins.h` only.

**Why per-board prefixes and not one set of names:** the prefix is a compile-time firewall.
`IDL0_PIN_I2C_SCL` is 21; on IDL1, GPIO21 is the onboard IMU chip-select. A port that carries a
stale macro into IDL1 code must fail to *build*, not drive the wrong pin. A board-prefixed macro
referenced under the wrong `CONFIG_IDL_BOARD_*` does exactly that.

**One addition beyond H4's letter — the neutral alias block.** H4 alone leaves shared code
(GPS UART, SPI bus init, speed task) with no name that compiles on both boards, which would force
an `#if` around every pin reference. Each board header therefore also defines **`IDL_PIN_*`
aliases, only for signals that exist on both boards with the same driver semantics** (the alias
sits next to the board's own numbers, so it can never go stale independently). Signals whose
*meaning* changed get **no alias on purpose** — referencing them from shared code is a compile
error that forces the `#if`:

| Neutral alias (both boards) | IDL0 value | IDL1 value |
|---|---|---|
| `IDL_PIN_SPI_SCK` / `_MOSI` / `_MISO` | 19 / 18 / 20 | 19 / 18 / 20 |
| `IDL_PIN_CS_SD` | 23 | **7** |
| `IDL_PIN_CS_IMU0` | 2 | **21** |
| `IDL_PIN_GPS_TX` / `_GPS_RX` | 16 / 17 | 16 / 17 |
| `IDL_PIN_SPEED_FRONT` / `_SPEED_REAR` | 4 / 6 | **0 / 1** |
| `IDL_PIN_LED_ONBOARD` | 15 (active-low) | 15 (active-low) |

No alias (board-divergent, `#if CONFIG_IDL_BOARD_*` required): buttons (ADC-ladder plan → digital
`USER_BTN`), pressure/analog (ESP32 ADC → TLA2528), I2C (IDL0's TEMP software-only pins → a real
bus on different SCL), `PWR_EN`, `BAT_SENSE`, the TLA channel and address constants (IDL1-only).
If the owner wants strict H4 with no alias layer, delete the alias blocks and gate every shared
reference instead — nothing else in this spec changes.

## 3. `main/pins_idl1.h` — finalized content

```c
/* IDL1 firmware pin assignments.
 *
 * Single source of truth for every IDL1 GPIO, TLA2528 channel, and
 * board-strap-derived I2C address. Derived from the KiCad netlist
 * (IDL1.net export 2026-08-31; see idl1-program/findings/2026-08-31-hw-delta/
 * pins-delta.md) under the rulings in idl1-program/decisions.md (H1, H2, H4,
 * P7-H3). Do not include this file directly: include "pins.h", which
 * selects the board header from CONFIG_IDL_BOARD_*.
 *
 * Namespace rules (load-bearing -- see H4):
 *   IDL1_PIN_*      real ESP32-C6 GPIOs, expand to GPIO_NUM_x (gpio_num_t).
 *                   The only macros ever passed to gpio_* / ADC / driver
 *                   pin-config APIs.
 *   IDL1_TLA_CH_*   TLA2528 channel numbers, plain ints. Passed ONLY to the
 *                   tla2528_* driver API. NEVER to gpio_*. TLA2528 channels
 *                   6 and 7 are named "GPIO6"/"GPIO7" in the TI datasheet;
 *                   they are unrelated to ESP32 GPIO6 (USER_BTN) and GPIO7
 *                   (SD_CS). A GPIO_NUM_7 "LED" macro would chip-select the
 *                   SD card.
 *   IDL1_I2C_ADDR_* 7-bit I2C addresses, plain ints. Passed ONLY as the
 *                   address argument of the I2C driver. Note 0x10 == 16
 *                   numerically; it is not GPIO16.
 */

#pragma once

#include "driver/gpio.h"

/* ---- Power sequencing ---------------------------------------------------- */
/* U2 (TPS22919) gates +3V3 to EVERYTHING except the MCU: GNSS, onboard IMU,
 * TLA2528 (and therefore both board LEDs and all analog inputs), microSD,
 * both M8 pod connectors, and the fused Qwiic port. Its ON pin is a
 * NON-LATCHING level input with no pull resistor on the net: configure
 * GPIO2 as output and drive it HIGH early in boot, and never let it revert
 * to an input, or the board quietly becomes an ESP32 with six dead
 * peripherals. (pins-delta.md call-out 12.) */
#define IDL1_PIN_PWR_EN           GPIO_NUM_2

/* Battery sense, ESP32 ADC1_CH4. Divider R2=1M (BAT+ side) / R3=2M (GND
 * side), C5 100nF: Vbat = 1.5 * Vpin (4.2 V -> 2.8 V at the pin). */
#define IDL1_PIN_BAT_SENSE        GPIO_NUM_4

/* ---- Status LED, onboard ------------------------------------------------- */
/* XIAO ESP32-C6 onboard user LED, ACTIVE-LOW. Kept per decisions.md H1:
 * both board LEDs are TLA2528 GPOs behind I2C, so this is the only
 * indicator before the bus is up, during hard faults, and when the TLA2528
 * does not answer (H2). */
#define IDL1_PIN_LED_ONBOARD      GPIO_NUM_15

/* ---- Shared SPI bus: onboard IMU + microSD only -------------------------- */
/* Pod IMUs left this bus; they are on I2C (see below). */
#define IDL1_PIN_SPI_SCK          GPIO_NUM_19  /* unchanged from IDL0 */
#define IDL1_PIN_SPI_MOSI         GPIO_NUM_18  /* unchanged from IDL0 */
#define IDL1_PIN_SPI_MISO         GPIO_NUM_20  /* unchanged from IDL0 */
#define IDL1_PIN_CS_SD            GPIO_NUM_7   /* SD_CS. Was 23 on IDL0; 23 is SCL now. */
#define IDL1_PIN_CS_IMU0          GPIO_NUM_21  /* IMU1_CS net, onboard LSM6DSV320X.
                                                * Was 2 on IDL0; 2 is PWR_EN now.
                                                * (Netlist ref "IMU1" = firmware IMU0.) */

/* ---- GNSS UART (u-blox SAM-M10Q; was MAX-M10S on IDL0) ------------------- */
#define IDL1_PIN_GPS_TX           GPIO_NUM_16  /* host TX -> module RX (GPS_RX_TO_MCU_TX) */
#define IDL1_PIN_GPS_RX           GPIO_NUM_17  /* module TX -> host RX (GPS_TX_TO_MCU_RX) */
/* EXTINT / RESET_N / TIMEPULSE / SAFEBOOT_N: unconnected, same as IDL0. */

/* ---- Wheel speed (Hall sensor is ON the pod: Allegro ATS601LSG) ---------- */
/* Per input: 4k7 pull-up to +3V3 + 10nF to GND at the pin, 330R series to
 * the M8, TVS channel on the field side. Edge-interrupt digital inputs. */
#define IDL1_PIN_WHEEL_F          GPIO_NUM_0   /* /MCU/WHEEL_F <- J5 pin 5 (front pod) */
#define IDL1_PIN_WHEEL_R          GPIO_NUM_1   /* /MCU/WHEEL_R <- J4 pin 5 (rear pod)  */

/* ---- Button (onboard SW1; IDL0's pod-harness /BUTTONS ladder is gone) ---- */
/* Single switch to GND, no external pull-up: enable the internal pull-up,
 * read active-low. Plain digital input, NOT an ADC ladder. */
#define IDL1_PIN_USER_BTN         GPIO_NUM_6

/* ---- I2C bus (multi-drop: TLA2528 + both pod IMUs + Qwiic J6) ------------ */
/* 4k7 pull-ups (R8/R9) on the MCU side; 33R series to the field nets; the
 * pods add no pull-ups of their own. Bus clock: the C6 datasheet lists
 * 100/400 kbit/s and the pods now hang on cables -- measure before running
 * faster (decisions.md P2, open item). */
#define IDL1_PIN_I2C_SDA          GPIO_NUM_22  /* same number as IDL0's TEMP macro by
                                                * coincidence; that was software-only. */
#define IDL1_PIN_I2C_SCL          GPIO_NUM_23  /* NOT 21. IDL0's TEMP I2C_SCL was 21;
                                                * GPIO21 is CS_IMU0 here. Never port it. */

/* ---- TLA2528 (U4) channel map -- plain ints, I2C-side, not ESP32 pins ---- */
/* Driver architecture (decisions.md P5): autonomous mode + auto-sequence
 * over AIN4/AIN5, OSR averaging, APPEND_STATUS on, OSC_SEL=0, results read
 * from RECENT_CHx as 16-bit averaged values. */
#define IDL1_TLA_CH_ANA_1         2   /* AIN2 <- /MCU/ANA_1, aux header J2 pin 1.
                                       * NOTE name/value skew: ANA_1 is channel 2.
                                       * Reserved for analog.channels[] (P5). */
#define IDL1_TLA_CH_ANA_2         3   /* AIN3 <- /MCU/ANA_2, aux header J2 pin 2. */
#define IDL1_TLA_CH_ANA_R         4   /* AIN4 <- rear pod analog, J4 pin 6.
                                       * Replaces IDL0_PIN_PRESSURE_REAR (ADC1_CH1). */
#define IDL1_TLA_CH_ANA_F         5   /* AIN5 <- front pod analog, J5 pin 6.
                                       * Replaces IDL0_PIN_PRESSURE_FRONT (ADC1_CH0).
                                       * ANA_F/ANA_R land on fixed registry channels
                                       * 20/21 (P5; which-is-which set in the spec). */
#define IDL1_TLA_CH_LED_RED       6   /* AIN6/GPO -> R1 330R -> D1 red anode, cathode
                                       * to GND: active-HIGH GPO. Role: RECORDING (H1). */
#define IDL1_TLA_CH_LED_GREEN     7   /* AIN7/GPO -> R5 330R -> D2 green anode:
                                       * active-HIGH GPO. Role: POWER + WIRELESS (H1).
                                       * "7" is a TLA channel. ESP32 GPIO7 is SD_CS. */
/* Channels 0/1 (AIN0/AIN1): unconnected on the board. */

/* ---- I2C addresses (board-strap-derived; 7-bit) -------------------------- */
/* TLA2528: ADDR is shorted to GND through copper net-tie NT1 -- an
 * UNSPECIFIED configuration (H2; not defective, just absent from TI's
 * table). 0x10 is the documented R1=DNP/R2=DNP row and what the board
 * becomes once NT1 is cut. Per H2: probe EXACTLY this address at boot,
 * confirm with a device-ID read, diag-log a loud distinct failure on NACK.
 * NO bus scan, NO sweep -- the owner resolves the real address manually at
 * bring-up; this constant is deliberately trivial to change while testing
 * candidates. Boot must proceed if it never ACKs (LEDs fall back to
 * IDL1_PIN_LED_ONBOARD). */
#define IDL1_I2C_ADDR_TLA         0x10

/* Pod IMUs (LSM6DSV320X, one per pod, same bus). SA0 has a 10k pull-down;
 * solder jumper JP1 shorts it to +3V3. Per P7-H3 (FINAL):
 *   front pod = JP1 OPEN   = 0x6A = spec/registry IMU1
 *   rear  pod = JP1 CLOSED = 0x6B = spec/registry IMU2
 * Confirm each with WHO_AM_I at boot; diag-log an absent pod, never guess. */
#define IDL1_I2C_ADDR_IMU_FRONT   0x6A
#define IDL1_I2C_ADDR_IMU_REAR    0x6B

/* ---- Removed from IDL0, no IDL1 equivalent ------------------------------- */
/* IDL0_PIN_CS_IMU1 (21) and IDL0_PIN_CS_IMU2 (22): pod IMUs are I2C devices
 * now, addressed above. GPIO21 and GPIO22 are reassigned (CS_IMU0, SDA).
 * IDL0_PIN_PRESSURE_FRONT (0) / _REAR (1): TLA channels now. GPIO0/1 are
 * reassigned (WHEEL_F, WHEEL_R). */

/* ---- Neutral aliases: signals shared with IDL0, same driver semantics ---- */
/* Shared code uses these; board-divergent code uses IDL1_PIN_* inside
 * CONFIG_IDL_BOARD_IDL1 blocks. See the board-selection spec, section 2. */
#define IDL_PIN_SPI_SCK           IDL1_PIN_SPI_SCK
#define IDL_PIN_SPI_MOSI          IDL1_PIN_SPI_MOSI
#define IDL_PIN_SPI_MISO          IDL1_PIN_SPI_MISO
#define IDL_PIN_CS_SD             IDL1_PIN_CS_SD
#define IDL_PIN_CS_IMU0           IDL1_PIN_CS_IMU0
#define IDL_PIN_GPS_TX            IDL1_PIN_GPS_TX
#define IDL_PIN_GPS_RX            IDL1_PIN_GPS_RX
#define IDL_PIN_SPEED_FRONT       IDL1_PIN_WHEEL_F
#define IDL_PIN_SPEED_REAR        IDL1_PIN_WHEEL_R
#define IDL_PIN_LED_ONBOARD       IDL1_PIN_LED_ONBOARD
```

## 4. Board selection

### 4.1 `main/Kconfig.projbuild` (new fragment; append if the file exists)

```
menu "IDL board"

    choice IDL_BOARD
        prompt "Target board"
        default IDL_BOARD_IDL0
        help
            Selects which board pin header main/pins.h includes.
            IDL0 is the board in hand; IDL1 is the rev with per-corner
            M8 pods, the TLA2528 analog front end, and switched 3V3.

    config IDL_BOARD_IDL0
        bool "IDL0 (rev 0)"

    config IDL_BOARD_IDL1
        bool "IDL1 (rev 1)"

    endchoice

endmenu
```

Default is IDL0 so the fork's first build is behaviorally identical to today's tree. For IDL1
builds, add `sdkconfig.defaults.idl1` containing `CONFIG_IDL_BOARD_IDL1=y` and build with
`idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.idl1"` — no hand-editing of
`sdkconfig`.

### 4.2 `main/pins.h` — reduced to the thin switch

```c
/* Board pin-map selector. Include THIS file, never a board header directly.
 * The real assignments live in pins_idl0.h / pins_idl1.h, chosen by the
 * Kconfig "IDL board" choice. */

#pragma once

#include "sdkconfig.h"

#if CONFIG_IDL_BOARD_IDL0
#include "pins_idl0.h"
#elif CONFIG_IDL_BOARD_IDL1
#include "pins_idl1.h"
#else
#error "No IDL board selected: set CONFIG_IDL_BOARD_IDL0 or CONFIG_IDL_BOARD_IDL1"
#endif
```

### 4.3 `main/pins_idl0.h`

Today's `pins.h` content, moved verbatim (`git mv main/pins.h main/pins_idl0.h`, then recreate
`pins.h` as above). All `IDL0_PIN_*` names and values unchanged, plus the matching neutral alias
block appended at the end:

```c
/* ---- Neutral aliases: signals shared with IDL1, same driver semantics ---- */
#define IDL_PIN_SPI_SCK           IDL0_PIN_SPI_SCK
#define IDL_PIN_SPI_MOSI          IDL0_PIN_SPI_MOSI
#define IDL_PIN_SPI_MISO          IDL0_PIN_SPI_MISO
#define IDL_PIN_CS_SD             IDL0_PIN_CS_SD
#define IDL_PIN_CS_IMU0           IDL0_PIN_CS_IMU0
#define IDL_PIN_GPS_TX            IDL0_PIN_GPS_TX
#define IDL_PIN_GPS_RX            IDL0_PIN_GPS_RX
#define IDL_PIN_SPEED_FRONT       IDL0_PIN_SPEED_FRONT
#define IDL_PIN_SPEED_REAR        IDL0_PIN_SPEED_REAR
#define IDL_PIN_LED_ONBOARD       IDL0_PIN_LED
```

### 4.4 Driver pattern

Drivers include `pins.h` only. Shared code uses `IDL_PIN_*`. Board-divergent code is gated and
uses the board's own prefix inside the gate:

```c
#include "pins.h"

#if CONFIG_IDL_BOARD_IDL1
    /* pressure: I2C read of TLA2528 RECENT_CHx for IDL1_TLA_CH_ANA_F / _ANA_R */
#else
    /* pressure: adc1_get_raw on IDL0_PIN_PRESSURE_FRONT / _REAR */
#endif
```

A stale macro under the wrong gate (`IDL0_PIN_I2C_SCL` in an IDL1 block, `IDL1_PIN_PWR_EN` in
IDL0 code) is an undefined identifier — a build failure, which is the point.

## 5. Namespace-collision audit (verified)

Three namespaces exist in `pins_idl1.h`. Checked macro by macro against the §3 content:

1. **Token discipline.** Every `IDL1_PIN_*` and `IDL_PIN_*` macro expands (directly or through one
   alias) to a `GPIO_NUM_x` token — `gpio_num_t` typed. Every `IDL1_TLA_CH_*` and `IDL1_I2C_ADDR_*`
   macro expands to a bare integer with no `GPIO_NUM_` anywhere. `GPIO_NUM_` appears in §3 only on
   `IDL1_PIN_*` definition lines. ✔
2. **Name discipline.** No `IDL1_TLA_CH_*` or `IDL1_I2C_ADDR_*` name contains `PIN` or `GPIO`; no
   `IDL1_PIN_*` name contains `TLA`, `CH_`, or `ADDR`. There is no `IDL1_PIN_LED` bare macro that
   could be mistaken for a board LED — the only PIN-namespace LED is `IDL1_PIN_LED_ONBOARD`
   (a real GPIO), and both board LEDs exist only as `IDL1_TLA_CH_LED_*`. ✔
3. **Live digit collisions the split protects** (same number, different chip):

   | number | as ESP32 GPIO (`IDL1_PIN_*`) | as TLA2528 channel (`IDL1_TLA_CH_*`) |
   |---|---|---|
   | 2 | `PWR_EN` — power switch | `ANA_1` aux input |
   | 4 | `BAT_SENSE` | `ANA_R` rear pod analog |
   | 6 | `USER_BTN` | `LED_RED` (recording) |
   | 7 | `CS_SD` — **the** collision | `LED_GREEN` (power/wireless) |

   (3 and 5 collide only with unused ESP32 GPIOs.) ✔
4. **Known residual hazards, mitigated by rule rather than by name:**
   - C's preprocessor and implicit int conversions mean `gpio_set_level(IDL1_TLA_CH_LED_GREEN, 1)`
     would still compile. Enforced discipline: the TLA2528 driver API takes channels as `uint8_t`
     parameters in `tla2528_*` functions only, and review greps
     `gpio_[a-z_]*\(.*\b(IDL1_TLA_CH|IDL1_I2C_ADDR|IDL_TLA)` (expect zero matches) and
     `GPIO_NUM_` in `pins_idl1.h` (expect matches only on `IDL1_PIN_*` lines).
   - `IDL1_I2C_ADDR_TLA` (0x10) equals 16, the GPS_TX GPIO, as a bare integer. 0x6A/0x6B exceed the
     C6's GPIO range and would be rejected by the GPIO driver; 0x10 would not — hence the same
     grep rule covers `IDL1_I2C_ADDR_*`.
   - `IDL1_TLA_CH_ANA_1` is channel **2** and `_ANA_2` is channel **3** (names follow the net/header
     labels, values follow the AIN channels). Flagged in the header comment.

## 6. Migration table — every `IDL0_PIN_*` macro

Transcription source for the port. "Rename only" means the number is unchanged and the call site
just moves to the IDL1 (or neutral) macro.

| IDL0 macro | value | outcome | IDL1 replacement | port action |
|---|---|---|---|---|
| `IDL0_PIN_LED` | 15 | kept (role narrowed) | `IDL1_PIN_LED_ONBOARD` / `IDL_PIN_LED_ONBOARD` | Rename. Still active-low. On IDL1 it is the pre-I2C / hard-fault / ADC-absent indicator only; board status LEDs move to the two `IDL1_TLA_CH_LED_*` GPOs (red = recording, green = power+wireless, both active-high, per H1). |
| `IDL0_PIN_BUTTONS` | 5 | renumbered + re-typed | `IDL1_PIN_USER_BTN` (6) | Digital read, internal pull-up, active-low. Delete any ADC-ladder plan. GPIO5 is unconnected on IDL1. |
| `IDL0_PIN_SPI_SCK` | 19 | kept | `IDL1_PIN_SPI_SCK` / `IDL_PIN_SPI_SCK` | Rename only. |
| `IDL0_PIN_SPI_MOSI` | 18 | kept | `IDL1_PIN_SPI_MOSI` / `IDL_PIN_SPI_MOSI` | Rename only. |
| `IDL0_PIN_SPI_MISO` | 20 | kept | `IDL1_PIN_SPI_MISO` / `IDL_PIN_SPI_MISO` | Rename only. |
| `IDL0_PIN_CS_SD` | 23 | renumbered | `IDL1_PIN_CS_SD` (7) | 23 is SCL on IDL1 — a carried-over 23 would clock the I2C bus with SD traffic. |
| `IDL0_PIN_CS_IMU0` | 2 | renumbered | `IDL1_PIN_CS_IMU0` (21) | 2 is PWR_EN on IDL1 — a carried-over 2 would toggle the peripheral power rail with every CS assert. |
| `IDL0_PIN_CS_IMU1` | 21 | **ceases to exist** | `IDL1_I2C_ADDR_IMU_FRONT` (0x6A) | Front pod IMU is an I2C device. Delete the macro and the SPI CS logic; the pod driver plan (NEXT-PROMPTS #2) owns the replacement driver. |
| `IDL0_PIN_CS_IMU2` | 22 | **ceases to exist** | `IDL1_I2C_ADDR_IMU_REAR` (0x6B) | Same; 22 is SDA on IDL1. |
| `IDL0_PIN_GPS_TX` | 16 | kept | `IDL1_PIN_GPS_TX` / `IDL_PIN_GPS_TX` | Rename only (module is SAM-M10Q now; UART unchanged). |
| `IDL0_PIN_GPS_RX` | 17 | kept | `IDL1_PIN_GPS_RX` / `IDL_PIN_GPS_RX` | Rename only. |
| `IDL0_PIN_SPEED_FRONT` | 4 | renumbered | `IDL1_PIN_WHEEL_F` (0) / `IDL_PIN_SPEED_FRONT` | 4 is BAT_SENSE on IDL1. New RC front end; edge-interrupt logic unchanged. |
| `IDL0_PIN_SPEED_REAR` | 6 | renumbered | `IDL1_PIN_WHEEL_R` (1) / `IDL_PIN_SPEED_REAR` | 6 is USER_BTN on IDL1. |
| `IDL0_PIN_PRESSURE_FRONT` | 0 | **becomes a TLA2528 channel** | `IDL1_TLA_CH_ANA_F` (ch 5) | ESP32 ADC read → I2C read of the 16-bit averaged `RECENT_CH5` result (P5). GPIO0 is WHEEL_F on IDL1. |
| `IDL0_PIN_PRESSURE_REAR` | 1 | **becomes a TLA2528 channel** | `IDL1_TLA_CH_ANA_R` (ch 4) | Same, `RECENT_CH4`. GPIO1 is WHEEL_R on IDL1. |
| `IDL0_PIN_I2C_SDA` (TEMP) | 22 | superseded by real hardware | `IDL1_PIN_I2C_SDA` (22) | Same number by coincidence only — IDL0's was a software reuse of CS_IMU2's pin with no bus. Delete the TEMP block and its removal note; the IDL1 bus is permanent. |
| `IDL0_PIN_I2C_SCL` (TEMP) | 21 | **renumbered — the trap** | `IDL1_PIN_I2C_SCL` (23) | A carried-over 21 would drive SCL on the onboard IMU's chip-select. This row is why the prefixes exist. |

New on IDL1, no IDL0 ancestor: `IDL1_PIN_PWR_EN` (2), `IDL1_PIN_BAT_SENSE` (4),
`IDL1_TLA_CH_ANA_1` (2), `IDL1_TLA_CH_ANA_2` (3), `IDL1_I2C_ADDR_TLA` (0x10).

## 7. Fork-time transcription steps

1. `git mv main/pins.h main/pins_idl0.h`; append the §4.3 alias block.
2. Create `main/pins.h` from §4.2 and `main/pins_idl1.h` from §3, verbatim.
3. Add the §4.1 Kconfig fragment and `sdkconfig.defaults.idl1`.
4. Build for IDL0 (default choice). Zero code changes yet ⇒ must compile and behave identically.
5. Walk the §6 table: for each IDL0 macro, edit its call sites per the "port action" column —
   rename-only rows mechanically, gated rows with `#if CONFIG_IDL_BOARD_IDL1`.
6. Build both configs. Run the §5.4 review greps.
7. New IDL1-only behavior (PWR_EN at boot, TLA2528 probe-one-address, pod WHO_AM_I checks) comes
   from the pod driver plan and decisions.md H2/P7·H3 — not from this header spec.

## 8. Open items that touch this file

- **H2 next spin:** NT1 → real 0603 pads to GND (11k = documented 0x11, or DNP = 0x10). If the
  owner's manual bring-up resolves the shorted board to something other than 0x10, edit
  `IDL1_I2C_ADDR_TLA` — one line, by design.
- **P2:** I2C bus clock is a bring-up measurement; nothing in this header encodes a speed.
- Registry mapping of `ANA_F`/`ANA_R` onto fixed channels 20/21 (P5) is a spec-side assignment;
  this header deliberately does not claim which is 20 and which is 21.
