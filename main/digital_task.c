#include "digital_task.h"

#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "idl1_format.h"
#include "pins.h"
#include "writer_task.h"

#define TAG "digital_task"

typedef struct {
    int     channel_idx;       /* index into s_channels */
    int64_t timestamp_us;
} press_event_t;

/* Event queue depth, events. Wheel pulses at ~725 Hz worst case per wheel
 * leave the task ~22 ms of slack at this depth; markers need far less. */
#define PRESS_QUEUE_DEPTH 32

static QueueHandle_t s_press_queue;
static TaskHandle_t  s_task;
static digital_channel_cfg_t s_channels[DIGITAL_TASK_MAX_CHANNELS];
static size_t  s_channel_count;
/* Accepted events per channel this session. Markers write it truncated to
 * u8 (wraps, as before); wheels write the full u32. */
static uint32_t s_press_counters[DIGITAL_TASK_MAX_CHANNELS];
static int64_t  s_last_press_us[DIGITAL_TASK_MAX_CHANNELS];
/* Wheel edges rejected by min_interval_us, per channel, count. */
static uint32_t s_rejected[DIGITAL_TASK_MAX_CHANNELS];
/* Edges lost because the queue was full, count (all channels). Written
 * only by the ISR. */
static volatile uint32_t s_queue_full;

/* T21 diag: marker CHANNEL_SAMPLE records the writer rejected (ring buffer
 * full), counted so a dropped press leaves a trace. Plain uint32_t —
 * single-core chip, increment-only diagnostics. */
static uint32_t s_diag_writer_drops;

static void IRAM_ATTR gpio_isr_handler(void *arg) {
    const int idx = (int)(intptr_t)arg;
    press_event_t ev = {
        .channel_idx = idx,
        .timestamp_us = esp_timer_get_time(),
    };
    BaseType_t hp_task_woken = pdFALSE;
    if (xQueueSendFromISR(s_press_queue, &ev, &hp_task_woken) != pdTRUE) {
        s_queue_full++;   /* reported at stop; no logging from an ISR */
    }
    if (hp_task_woken) portYIELD_FROM_ISR();
}

/* Submit one CHANNEL_SAMPLE record via the writer: `value_len` bytes of
 * `value` (1 for a marker's u8, 4 for a wheel's u32). Builds the framed
 * record on the stack. */
static void submit_press_record(uint16_t channel_id, int64_t ts_us,
                                const void *value, size_t value_len) {
    /* 3-byte framing + 2 channel_id + 8 timestamp_us + up to 4 value = 17 bytes. */
    uint8_t buf[20];
    const size_t n = idl1_format_channel_sample(buf, channel_id, ts_us,
                                                value, value_len);
    if (n == 0) return;
    if (!idl0_writer_submit(buf, n)) {
        s_diag_writer_drops++;   /* T21 — no log on the path; see digital_task_diag_writer_drops() */
    }
}

/* T22: per-event record-building, shared by digital_task_fn's normal loop
 * and digital_task_stop's drain-on-stop loop so a press queued right before
 * stop gets the exact same debounce + submit treatment as one processed
 * live. */
static void process_press_event(const press_event_t *ev) {
    if (ev->channel_idx < 0 || (size_t)ev->channel_idx >= s_channel_count) return;
    const digital_channel_cfg_t *ch = &s_channels[ev->channel_idx];
    const int64_t since_us = ev->timestamp_us - s_last_press_us[ev->channel_idx];

    if (ch->kind == DIGITAL_KIND_WHEEL) {
        /* Glitch guard, not a debounce: an edge inside min_interval_us is
         * not a tooth. Counted so a noisy input shows at stop. */
        if (since_us < (int64_t)ch->min_interval_us) {
            s_rejected[ev->channel_idx]++;
            return;
        }
        s_last_press_us[ev->channel_idx] = ev->timestamp_us;
        /* Monotonic u32 pulse count (spec §5.2 WheelFront/WheelRear). */
        const uint32_t count = ++s_press_counters[ev->channel_idx];
        submit_press_record(ch->channel_id, ev->timestamp_us, &count, sizeof(count));
        return;
    }

    /* Software debounce — reject presses within debounce_ms of the last one. */
    if (since_us < (int64_t)ch->debounce_ms * 1000) return;
    s_last_press_us[ev->channel_idx] = ev->timestamp_us;

    /* Monotonic press counter (u8 — wraps; press order still derivable). */
    const uint8_t presses = (uint8_t)++s_press_counters[ev->channel_idx];
    /* T09 — this task only runs during a session; deleted (not
     * downgraded) the per-press ESP_LOGI that used to run here.
     * ESP_LOGD still costs a runtime tag-level check on the logging
     * path, and a dev build with DEBUG level enabled would print it, so
     * only removing it fully honours "zero processing while logging"
     * (§2). Marker presses aren't traced anywhere post-session either;
     * they're recoverable from the session file's CHANNEL_SAMPLE
     * records if ever needed. */
    submit_press_record(ch->channel_id, ev->timestamp_us, &presses, sizeof(presses));
}

static void digital_task_fn(void *arg) {
    (void)arg;
    press_event_t ev;
    while (true) {
        if (xQueueReceive(s_press_queue, &ev, portMAX_DELAY) != pdTRUE) continue;
        process_press_event(&ev);
    }
}

/* Is `pin` one of the board's fixed wheel inputs? kind=WHEEL is built by
 * session.c from pins.h, never from app-supplied pin numbers, but checked
 * anyway: a wheel channel configures its pin with no pulls. */
static bool wheel_pin_allowed(int pin) {
    return pin == (int)IDL1_PIN_WHEEL_F || pin == (int)IDL1_PIN_WHEEL_R;
}

/* Is `pin` safe to hand to a config-defined marker input? The pin number
 * comes from idl0_config.json (app-supplied), so it must never land on a
 * GPIO the board already drives: above all IDL1_PIN_PWR_EN — reconfiguring
 * it as an input switches off every peripheral mid-session (pins.h) — but
 * also the bus, chip-select, UART and LED pins, and the wheel inputs
 * (externally pulled up and owned by kind=WHEEL). */
static bool marker_pin_allowed(int pin) {
    if (pin < 0 || !GPIO_IS_VALID_GPIO(pin)) return false;
    static const gpio_num_t board_owned[] = {
        IDL1_PIN_PWR_EN, IDL1_PIN_BAT_SENSE, IDL1_PIN_LED_ONBOARD,
        IDL1_PIN_SPI_SCK, IDL1_PIN_SPI_MOSI, IDL1_PIN_SPI_MISO,
        IDL1_PIN_CS_SD, IDL1_PIN_CS_IMU0,
        IDL1_PIN_GPS_TX, IDL1_PIN_GPS_RX,
        IDL1_PIN_I2C_SDA, IDL1_PIN_I2C_SCL,
        IDL1_PIN_WHEEL_F, IDL1_PIN_WHEEL_R,
    };
    for (size_t i = 0; i < sizeof(board_owned) / sizeof(board_owned[0]); i++) {
        if (pin == (int)board_owned[i]) return false;
    }
    return true;
}

void digital_task_start(const digital_channel_cfg_t *channels, size_t count) {
    digital_task_stop();
    if (channels == NULL || count == 0) return;

    if (count > DIGITAL_TASK_MAX_CHANNELS) count = DIGITAL_TASK_MAX_CHANNELS;
    s_channel_count = count;
    memcpy(s_channels, channels, count * sizeof(*channels));
    memset(s_press_counters, 0, sizeof(s_press_counters));
    memset(s_last_press_us, 0, sizeof(s_last_press_us));
    memset(s_rejected, 0, sizeof(s_rejected));
    s_queue_full = 0;

    s_press_queue = xQueueCreate(PRESS_QUEUE_DEPTH, sizeof(press_event_t));
    if (s_press_queue == NULL) {
        ESP_LOGE(TAG, "queue create failed");
        return;
    }

    /* Best-effort install — already-installed is fine. */
    gpio_install_isr_service(0);

    for (size_t i = 0; i < count; i++) {
        digital_channel_cfg_t *ch = &s_channels[i];
        if (!ch->enabled) continue;
        if (ch->kind == DIGITAL_KIND_WHEEL) {
            if (!wheel_pin_allowed(ch->gpio_pin)) {
                ESP_LOGE(TAG, "wheel '%s' on GPIO %d refused: not a wheel input",
                         ch->label, ch->gpio_pin);
                ch->enabled = false;
                continue;
            }
            /* No internal pulls: 4k7 to +3V3 + 10 nF on the board (pins.h).
             * ATS601 is open-drain, so one falling edge per tooth. */
            const gpio_config_t wcfg = {
                .pin_bit_mask = 1ULL << ch->gpio_pin,
                .mode         = GPIO_MODE_INPUT,
                .pull_up_en   = GPIO_PULLUP_DISABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type    = GPIO_INTR_NEGEDGE,
            };
            gpio_config(&wcfg);
            gpio_isr_handler_add(ch->gpio_pin, gpio_isr_handler, (void *)(intptr_t)i);
            ESP_LOGI(TAG, "wheel '%s' on GPIO %d (falling edge, min interval %lu us)",
                     ch->label, ch->gpio_pin, (unsigned long)ch->min_interval_us);
            continue;
        }
        if (!marker_pin_allowed(ch->gpio_pin)) {
            ESP_LOGW(TAG, "marker '%s' on GPIO %d refused: invalid or board-owned pin",
                     ch->label, ch->gpio_pin);
            ch->enabled = false;   /* also keeps digital_task_stop() off it */
            continue;
        }
        const gpio_config_t cfg = {
            .pin_bit_mask = 1ULL << ch->gpio_pin,
            .mode         = GPIO_MODE_INPUT,
            .pull_up_en   = ch->active_low ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
            .pull_down_en = ch->active_low ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE,
            .intr_type    = ch->active_low ? GPIO_INTR_NEGEDGE : GPIO_INTR_POSEDGE,
        };
        gpio_config(&cfg);
        gpio_isr_handler_add(ch->gpio_pin, gpio_isr_handler, (void *)(intptr_t)i);
        ESP_LOGI(TAG, "marker '%s' on GPIO %d (active_%s, debounce %u ms)",
                 ch->label, ch->gpio_pin,
                 ch->active_low ? "low" : "high", ch->debounce_ms);
    }

    xTaskCreate(digital_task_fn, "digital_task", 2048, NULL, 5, &s_task);
}

void digital_task_stop(void) {
    for (size_t i = 0; i < s_channel_count; i++) {
        if (s_channels[i].enabled) {
            gpio_isr_handler_remove(s_channels[i].gpio_pin);
        }
    }

    /* T22: drain any press already enqueued but not yet consumed BEFORE
     * deleting the task/queue below — otherwise a press that landed in the
     * queue just before stop (possibly the last marker press of the
     * session) is silently destroyed by vQueueDelete. Non-blocking receive
     * (0 ticks to wait): only events already sitting in the queue right
     * now are drained; ISRs were just detached above so no more will
     * arrive. Each drained event goes through the same record-building
     * path as digital_task_fn, so the writer-drop counter (T21) still
     * accounts for it if the writer rejects it. */
    if (s_press_queue != NULL) {
        press_event_t ev;
        while (xQueueReceive(s_press_queue, &ev, 0) == pdTRUE) {
            process_press_event(&ev);
        }
    }

    /* Session is over (or never started): safe to log. */
    for (size_t i = 0; i < s_channel_count; i++) {
        const digital_channel_cfg_t *ch = &s_channels[i];
        if (!ch->enabled || ch->kind != DIGITAL_KIND_WHEEL) continue;
        ESP_LOGI(TAG, "wheel '%s': %lu pulses, %lu rejected (< %lu us apart)",
                 ch->label, (unsigned long)s_press_counters[i],
                 (unsigned long)s_rejected[i], (unsigned long)ch->min_interval_us);
    }
    if (s_queue_full > 0) {
        ESP_LOGW(TAG, "%lu edges lost to a full event queue", (unsigned long)s_queue_full);
    }

    if (s_task != NULL) {
        vTaskDelete(s_task);
        s_task = NULL;
    }
    if (s_press_queue != NULL) {
        vQueueDelete(s_press_queue);
        s_press_queue = NULL;
    }
    s_channel_count = 0;
}

uint32_t digital_task_diag_writer_drops(void)
{
    return s_diag_writer_drops;
}

void digital_task_diag_reset(void)
{
    s_diag_writer_drops = 0;
}
