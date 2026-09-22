/* IDL1 board LED roles (decisions.md H1), driven through the TLA2528.
 *
 *   green (D2) — power + wireless: solid while the phone is connected over
 *                BLE or the WiFi AP is up; slow blink (1 Hz, 50 %) while
 *                powered with no wireless link.
 *   red   (D1) — recording: solid while a logging session runs, else off.
 *
 * The XIAO onboard LED (GPIO15, led_status.c) is unchanged: it covers the
 * window before the I2C bus is up and every boot where the TLA2528 does
 * not answer (H2).
 */

#pragma once

/* Start the LED role timer. No-op (logs once) if the TLA2528 is not up —
 * then the onboard LED is the only indicator. Call after tla2528_init(). */
void idl1_board_leds_start(void);
