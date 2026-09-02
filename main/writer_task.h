/* Single-consumer SD writer.
 *
 * Sensor tasks (gps_task, imu_task, …) submit records into the ring
 * buffer; one writer_task drains it and calls idl0_sd_append. Only one
 * task ever touches the open SD file — no per-record SD mutex needed
 * outside sd_logger itself.
 *
 * Buffer absorbs SD-write latency transients without dropping records. 64 KB
 * is ~3 s of one IMU at 833 Hz (~20 KB/s) or ~1 s of three — enough to ride out
 * the fresh-file FAT-allocation slowness at session start that was overflowing
 * the old 16 KB ring in the first few seconds.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define IDL0_WRITER_BUFFER_BYTES (64 * 1024)

/* Create the ring buffer and start the writer task. Idempotent. */
bool idl0_writer_init(void);

/* Submit a fully-framed record. Drops on backpressure (returns false) —
 * sensor producers are expected to count+continue on drop (no per-drop log
 * on the logging path; see each producer's own diag-drop accessor). */
bool idl0_writer_submit(const uint8_t *buf, size_t len);

/* Block up to `timeout_ms` for the buffer to drain. Returns true if
 * empty, false on timeout. */
bool idl0_writer_drain(uint32_t timeout_ms);

/* Diagnostic accessor (T05): count of idl0_sd_append failures since the last
 * idl0_writer_diag_reset() call. Units: count (failed appends). */
uint32_t idl0_writer_diag_append_failures(void);

/* Zero the append-failure counter. Called by the session layer at
 * idl0_session_start() so each session's summary starts from zero. */
void idl0_writer_diag_reset(void);

/* T04: true once an idl0_sd_append failure has been observed while
 * idl0_sd_state() reads IDL0_SD_FULL or IDL0_SD_ERROR. Polled by the 1 Hz
 * status timer (status.c) to trigger stop-and-report (§10.2 / A7). */
bool idl0_writer_sd_fault(void);

/* Clear the SD-fault flag. Called by idl0_session_start() alongside the
 * other diag resets so each session starts fault-free. */
void idl0_writer_clear_sd_fault(void);
