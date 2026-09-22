/* IDL1 boot self-test report (bring-up phase 1).
 *
 * Answers "is every chip alive?" in one delimited console block, one row
 * per chip / signal, each PASS / FAIL / ABSENT with the raw value behind
 * it. docs/bringup.md tells the owner what each failure means.
 */

#pragma once

/* How long the report watches the GPS UART (and the user button) before
 * printing, ms. The module emits NMEA at 1 Hz, so 3 s spans ~3 bursts. */
#define IDL1_BRINGUP_WINDOW_MS 3000

/* Run the self-test and print the report. Call once from app_main after
 * every subsystem has been initialised (it reads their boot results; it
 * does not re-probe anything except the battery ADC and USER_BTN, which
 * have no driver yet). Blocks for IDL1_BRINGUP_WINDOW_MS. Also appends one
 * condensed line to the on-SD diag log when that log is available. */
void idl1_bringup_report(void);
