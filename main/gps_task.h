/* GPS reader task.
 *
 * Reads bytes from the GPS UART, accumulates them into NMEA lines,
 * feeds them to the gps_driver integrator, and submits GPS_FIX records
 * (§5.6) into writer_task. On first fix, anchors the session's wall
 * clock and renames the session file via session.c. Watches for lost
 * lock (no valid RMC for >5 s) and pushes FIX → NOFIX in that case.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Brings up the UART (via idl0_gps_init) and starts the reader task.
 * Idempotent. */
bool idl0_gps_task_start(void);

/* Diagnostic accessor (T08): count of GPS_FIX records the writer rejected
 * since the last gps_task_diag_reset() call. Units: count. */
uint32_t gps_task_diag_writer_drops(void);

/* Zero the writer-drop counter. Called by the session layer at
 * idl0_session_start(). */
void gps_task_diag_reset(void);

/* Boot self-test counters (bringup.c), monotonic since boot, never reset.
 * Units: bytes read from the GPS UART / NMEA lines starting with '$'
 * (checksum not verified — this counts traffic, not valid fixes). */
uint32_t gps_task_diag_rx_bytes(void);
uint32_t gps_task_diag_rx_sentences(void);
