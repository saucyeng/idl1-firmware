/* USER_BTN (SW1, GPIO6, active-low, internal pull-up).
 *
 * Short press (< IDL1_BTN_LONG_MS): toggle recording — same path as the BLE
 * START/STOP_LOGGING commands.
 * Long press (>= IDL1_BTN_LONG_MS): power off — stop any session, drop the
 * switched rail (PWR_EN low, held), deep sleep until the button is pressed.
 *
 * The button arms only after it has been seen released, so a line held low
 * at boot (stuck switch, bridge) can never power the board straight off.
 * Every event is logged to the console under the "button" tag. */

#pragma once

#define IDL1_BTN_LONG_MS 2000

/* Call once, late in boot. */
void idl1_button_start(void);

/* Powers the logger off from the app (BLE CMD_POWER_OFF): the long-press
 * shutdown, run on its own task a moment later so the command's ACK goes
 * out first. Returns at once. */
void idl1_power_off_request(void);

/* Call first thing in app_main: releases the PWR_EN deep-sleep hold left by
 * a button power-off so the rail can be driven high again. */
void idl1_button_release_sleep_holds(void);
