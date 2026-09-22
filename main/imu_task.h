/* IMU polling task.
 *
 * Periodically reads FIFO_STATUS, drains all available FIFO words via
 * the IMU driver, builds (gyro, accel) sample pairs, assigns per-
 * sample timestamps walking back from the read instant at the nominal
 * ODR cadence (§5.5), and submits one IMU_SAMPLE record per pair into
 * the writer pipeline when a session is running.
 *
 * Outside a session the task still drains the FIFO so it doesn't
 * overflow (the §7.3 status string surfaces overruns when they occur
 * — §5.5: not written to the file). */

#pragma once

#include <stdbool.h>

#include "imu_driver.h"   /* idl0_imu_state_t */

/* Brings up IMU0 from config and starts the polling task. Idempotent. */
bool idl0_imu_task_start(void);

/* Drain-instrumentation hooks for the session layer. The IMU task accumulates
 * per-drain timing in RAM during a session; the session layer zeroes it at
 * start and, at stop (after the session file closes), flushes a summary to the
 * on-SD diag log — so nothing is written mid-session (§1: raw capture only). */
void idl0_imu_diag_reset(void);
void idl0_imu_diag_flush(void);

/* T32: whole-device IMU aggregate — walks every IMU this task configured at
 * boot (imu_task owns the bus map, s_imu_bus[]) through the correct bus's
 * driver (SPI: imu_driver.c; I²C: imu_driver_i2c.c) and combines them with
 * the same OK/PARTIAL/ERROR/ABSENT semantics as imu_driver.c's
 * idl0_imu_state() (which only sees the SPI side). status.c calls this one
 * for the §7.3 "IMU:" status token. */
idl0_imu_state_t idl0_imu_task_state(void);

/* Boot WHO_AM_I probe result for IMU `imu_index` (0 = onboard SPI, 1 =
 * front pod 0x6A, 2 = rear pod 0x6B), from the driver that owns its bus.
 * NOT_RUN if the IMU is disabled in config or the index is out of range.
 * Read by the boot self-test report (bringup.c). */
idl0_imu_probe_t idl0_imu_task_probe(uint8_t imu_index);

/* T31: re-run init once for every configured IMU currently latched down
 * (SPI ERROR / I²C inactive-after-attempt), using the same per-IMU config
 * this task applied at boot. A transient bus error otherwise leaves an IMU
 * dead for the rest of boot (the only place its state goes back to OK/true
 * is inside the init functions, called once at task start). Called by
 * idl0_session_start() (session.c) right before that session's channel
 * registry is built. ESP_LOGW's the outcome per IMU retried (session-start
 * path — allowed); a no-op for IMUs that are already healthy or were never
 * configured.
 *
 * Cross-task invariant (no lock backs this — read before changing either
 * side): this runs on the BLE/session-start task while imu_task_fn's 20 ms
 * drain loop runs concurrently on the IMU task. That's only safe because
 * this must only be called for an IMU whose state is ERROR/inactive, and
 * drain_one()'s imu_is_active() gate means the polling task never touches
 * such an IMU's handle — the OK→non-OK transition happens synchronously
 * inside the drain itself, so the two tasks never contend for the same
 * device handle. If either gate ever changes (e.g. imu_is_active() starts
 * tolerating a non-OK state, or this function starts retrying an IMU that
 * isn't down), the device-handle remove/re-add in idl0_imu_init_one*
 * (imu_driver.c / imu_driver_i2c.c) can race the drain loop into a
 * use-after-free on the handle. */
void idl0_imu_task_reinit_failed(void);
