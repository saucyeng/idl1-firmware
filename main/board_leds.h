/* IDL1 board LED roles (decisions.md H1; patterns approved by Isaac
 * 2026-10-17), driven through the TLA2528.
 *
 *   booting                 red/green alternate fast (every 200 ms) until the
 *                           self-test is done
 *   on, no phone            green slow blink (1 Hz)
 *   phone on Bluetooth      green solid
 *   WiFi mode, waiting      green double-blink once a second
 *   files transferring      green fast flicker (5 Hz) while bytes move
 *   firmware arriving       red/green alternate slowly (1 Hz)
 *   firmware being checked  red + green solid
 *   recording               red solid; green as above for the link
 *
 * Recording always ends WiFi mode, so the WiFi patterns never overlap it.
 * The pattern is written only when it changes; a write that fails (the bus
 * is shared with the pods) is retried on the next tick.
 *
 * The XIAO onboard LED (GPIO15, led_status.c) is unchanged: it covers the
 * window before the I2C bus is up and every boot where the TLA2528 does
 * not answer (H2).
 */

#pragma once

/* Start the LED task. No-op (logs once) if the TLA2528 is not up — then
 * the onboard LED is the only indicator. Call after tla2528_init(). */
void idl1_board_leds_start(void);

/* Ends the boot pattern. Call once the boot self-test has run. */
void idl1_board_leds_set_booted(void);
