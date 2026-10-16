/* Debounced GPIO marker reader and wheel-speed pulse capture.
 *
 * Reads `config.digital.channels[]` (§8) entries with kind=`marker` and
 * registers a GPIO ISR per pin. Each accepted (post-debounce) press is
 * timestamped at the ISR and written to the standard writer pipeline as
 * a CHANNEL_SAMPLE (§5.7) on the channel id assigned by session.c at
 * session start. Value = monotonic press counter (u8, wraps at 255).
 *
 * Wheel speed (PT18/PT19, `wheel_speed.front/rear`, §3.5) rides the same
 * ISR -> queue -> task path as kind=WHEEL: one CHANNEL_SAMPLE per falling
 * edge on the board's fixed wheel pins, value = u32 monotonic pulse count,
 * timestamp taken in the ISR. Speed is computed app-side, never here.
 *
 * DigitalKind.level / .pwm are reserved in the schema but not implemented.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* User-added marker channels from `digital.channels[]` (§8). */
#define DIGITAL_MAX_CHANNELS 4

/* Fixed wheel inputs: front and rear (pins.h IDL1_PIN_WHEEL_F/_R). */
#define DIGITAL_WHEEL_CHANNELS 2

/* Everything digital_task_start() accepts in one call. */
#define DIGITAL_TASK_MAX_CHANNELS (DIGITAL_MAX_CHANNELS + DIGITAL_WHEEL_CHANNELS)

/* Shortest accepted gap between two wheel edges, us. Rejects anything
 * above 5 kHz -- a 60-tooth ring at 12 rev/s is ~725 Hz -- while the
 * 4k7 + 10 nF input network's ~100 us edge gives one clean edge per tooth
 * well inside it. Not the marker debounce_ms: 1 ms would drop real pulses
 * at 725 Hz. */
#define DIGITAL_WHEEL_MIN_INTERVAL_US 200

typedef enum {
    DIGITAL_KIND_MARKER = 0,
    DIGITAL_KIND_WHEEL,
} digital_kind_t;

typedef struct {
    bool           enabled;
    digital_kind_t kind;
    int            gpio_pin;
    bool           active_low;      /* MARKER only; WHEEL is always falling-edge */
    uint16_t       debounce_ms;     /* MARKER only, ms */
    uint32_t       min_interval_us; /* WHEEL only, us: reject an edge this soon after the last */
    char           label[20];       /* matches channel registry name field */
    uint8_t        channel_id;      /* assigned at session start */
} digital_channel_cfg_t;

/* Start the GPIO ISR-backed reader for markers and wheel inputs (up to
 * DIGITAL_TASK_MAX_CHANNELS). Idempotent — calling again with a new config
 * replaces the active channel set. Pass NULL/0 to stop. */
void digital_task_start(const digital_channel_cfg_t *channels, size_t count);

/* Stop the task and detach all installed ISRs. Logs each wheel channel's
 * pulse count for the session (stop path, not the logging path). */
void digital_task_stop(void);

/* Diagnostic accessor (T21): count of marker and wheel CHANNEL_SAMPLE records the
 * writer rejected since the last digital_task_diag_reset() call. Units: count. */
uint32_t digital_task_diag_writer_drops(void);

/* Zero the writer-drop counter. Called by the session layer at
 * idl0_session_start(). */
void digital_task_diag_reset(void);
