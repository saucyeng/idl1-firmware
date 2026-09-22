/* IDL1 firmware pin assignments.
 *
 * Single source of truth for every IDL1 GPIO, TLA2528 channel, and
 * board-strap-derived I2C address. Derived from the KiCad netlist
 * (IDL1.net export 2026-08-31; see docs/program/findings/2026-08-31-hw-delta/
 * pins-delta.md) under the rulings in docs/program/decisions.md (H1, H2, H4,
 * P7-H3, F2). Transcribed from docs/program/findings/2026-09-02-pins-idl1/
 * pins_idl1-spec.md section 3, minus its neutral IDL_PIN_* alias block and
 * with corrections R2/R3 of the bring-up phases file applied. This firmware
 * is IDL1-only (F2): there is no board selector and no IDL0 pin map.
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
/* Level that lights IDL1_PIN_LED_ONBOARD (0 = active-low). A logic level,
 * not a pin. */
#define IDL1_LED_ONBOARD_ACTIVE_LEVEL 0

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
 * pods add no pull-ups of their own. Bus clock: 400 kHz -- the C6 datasheet
 * lists only 100/400 kbit/s (see main/i2c_bus.c). */
#define IDL1_PIN_I2C_SDA          GPIO_NUM_22  /* same number as IDL0's TEMP macro by
                                                * coincidence; that was software-only. */
#define IDL1_PIN_I2C_SCL          GPIO_NUM_23  /* NOT 21. IDL0's TEMP I2C_SCL was 21;
                                                * GPIO21 is CS_IMU0 here. Never port it. */

/* ---- TLA2528 (U4) channel map -- plain ints, I2C-side, not ESP32 pins ---- */
/* Driver architecture (decisions.md P5 correction): auto-sequence mode over
 * AIN4/AIN5, host-initiated conversions, results fetched with bare I2C
 * reads (OSR averaging optional, APPEND_STATUS on, OSC_SEL=0). The TLA2528
 * has NO autonomous mode and NO RECENT_CHx registers -- its register map
 * ends at 0x12 (AUTO_SEQ_CH_SEL). */
#define IDL1_TLA_CH_ANA_1         2   /* AIN2 <- /MCU/ANA_1, aux header J2 pin 1.
                                       * NOTE name/value skew: ANA_1 is channel 2.
                                       * Reserved for analog.channels[] (P5). */
#define IDL1_TLA_CH_ANA_2         3   /* AIN3 <- /MCU/ANA_2, aux header J2 pin 2. */
#define IDL1_TLA_CH_ANA_R         4   /* AIN4 <- rear pod analog, J4 pin 6.
                                       * Replaces IDL0's PRESSURE_REAR (ADC1_CH1). */
#define IDL1_TLA_CH_ANA_F         5   /* AIN5 <- front pod analog, J5 pin 6.
                                       * Replaces IDL0's PRESSURE_FRONT (ADC1_CH0).
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
 * confirm with a SYSTEM_STATUS read (the part has no device-ID register,
 * R8), log a loud distinct failure on NACK. NO bus scan, NO sweep -- the
 * owner resolves the real address manually at bring-up; this constant is
 * deliberately trivial to change while testing candidates. Boot must
 * proceed if it never ACKs (LEDs fall back to IDL1_PIN_LED_ONBOARD). */
#define IDL1_I2C_ADDR_TLA         0x10

/* Pod IMUs (LSM6DSV320X, one per pod, same bus). SA0 has a 10k pull-down;
 * solder jumper JP1 shorts it to +3V3. Per P7-H3 (FINAL):
 *   front pod = JP1 OPEN   = 0x6A = spec/registry IMU1
 *   rear  pod = JP1 CLOSED = 0x6B = spec/registry IMU2
 * Confirm each with WHO_AM_I at boot; diag-log an absent pod, never guess. */
#define IDL1_I2C_ADDR_IMU_FRONT   0x6A
#define IDL1_I2C_ADDR_IMU_REAR    0x6B

/* ---- Removed from IDL0, no IDL1 equivalent ------------------------------- */
/* IDL0's CS_IMU1 (21) and CS_IMU2 (22): pod IMUs are I2C devices now,
 * addressed above. GPIO21 and GPIO22 are reassigned (CS_IMU0, SDA).
 * IDL0's PRESSURE_FRONT (0) / _REAR (1): TLA channels now. GPIO0/1 are
 * reassigned (WHEEL_F, WHEEL_R). */
