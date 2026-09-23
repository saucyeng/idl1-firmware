/* See button.h. */

#include "button.h"

#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ble_service.h"
#include "pins.h"
#include "session.h"
#include "status.h"
#include "tla2528.h"

static const char *TAG = "button";

#define BTN_POLL_MS        20
#define BTN_DEBOUNCE_MS    40    /* level must be stable this long to count */
#define BTN_ARM_HIGH_MS    200   /* released this long before the button arms */
#define BTN_STUCK_WARN_MS  5000  /* never armed after this -> warn once */

static bool pressed_raw(void)
{
    return gpio_get_level(IDL1_PIN_USER_BTN) == 0;   /* active-low */
}

static void toggle_recording(void)
{
    const bool running = idl0_session_is_running();
    const idl0_ble_command_t cmd = running ? IDL0_CMD_STOP_LOGGING
                                           : IDL0_CMD_START_LOGGING;
    ESP_LOGI(TAG, "short press -> %s", running ? "STOP recording" : "START recording");
    const uint8_t ack = idl0_session_on_ble_command(cmd);
    ESP_LOGI(TAG, "%s ack=0x%02X", running ? "stop" : "start", ack);
    idl0_status_publish();
}

/* Does not return. PWR_EN is deliberately driven LOW here — the single
 * sanctioned exception to "never touch PWR_EN" (main.c): powering off is the
 * point. The hold keeps it low through deep sleep; the next boot releases it
 * in idl1_button_release_sleep_holds() before driving it high. */
static void power_off(void)
{
    ESP_LOGW(TAG, "long press -> POWER OFF");
    if (idl0_session_is_running()) {
        ESP_LOGI(TAG, "stopping session before power off");
        idl0_session_stop();
    }
    tla2528_led_set(false, false);

    /* Wait for release: the wake source is "button low", so sleeping while
     * it is still held would wake immediately. */
    while (pressed_raw()) {
        vTaskDelay(pdMS_TO_TICKS(BTN_POLL_MS));
    }
    vTaskDelay(pdMS_TO_TICKS(BTN_DEBOUNCE_MS));

    gpio_set_level(IDL1_PIN_PWR_EN, 0);
    gpio_hold_en(IDL1_PIN_PWR_EN);

    /* GPIO6 is an LP IO on the C6: ext1 can wake on it. Keep the pull-up on
     * in sleep so the idle level stays high. */
    gpio_pullup_en(IDL1_PIN_USER_BTN);
    gpio_hold_en(IDL1_PIN_USER_BTN);
    esp_sleep_enable_ext1_wakeup_io(1ULL << IDL1_PIN_USER_BTN, ESP_EXT1_WAKEUP_ANY_LOW);

    ESP_LOGW(TAG, "rail off, deep sleep; press the button to power on");
    vTaskDelay(pdMS_TO_TICKS(50));   /* let the log drain */
    esp_deep_sleep_start();
}

static void button_task(void *arg)
{
    (void)arg;
    bool armed = false;
    bool stuck_warned = false;
    bool stable = pressed_raw();
    bool last_raw = stable;
    int64_t raw_since = esp_timer_get_time();
    int64_t press_start = 0;
    bool long_fired = false;
    const int64_t boot_us = raw_since;

    ESP_LOGI(TAG, "USER_BTN GPIO%d at start: %s", IDL1_PIN_USER_BTN,
             stable ? "LOW (pressed/held)" : "HIGH (released)");

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(BTN_POLL_MS));
        const int64_t now = esp_timer_get_time();
        const bool raw = pressed_raw();
        if (raw != last_raw) {
            last_raw = raw;
            raw_since = now;
            /* Raw edge trace, armed or not: shows exactly what the pin does
             * (e.g. a normally-closed switch reads LOW at rest). */
            ESP_LOGI(TAG, "GPIO%d raw -> %s", IDL1_PIN_USER_BTN, raw ? "LOW" : "HIGH");
        }
        const int64_t stable_ms = (now - raw_since) / 1000;

        if (!armed) {
            if (!raw && stable_ms >= BTN_ARM_HIGH_MS) {
                armed = true;
                stable = false;
                ESP_LOGI(TAG, "armed (released seen); short=record, long>=%d ms=power off",
                         IDL1_BTN_LONG_MS);
            } else if (!stuck_warned && (now - boot_us) / 1000 >= BTN_STUCK_WARN_MS) {
                stuck_warned = true;
                ESP_LOGE(TAG, "USER_BTN has read LOW since boot; button disabled "
                              "until it reads released (stuck switch / short to GND?)");
            }
            continue;
        }

        if (raw != stable && stable_ms >= BTN_DEBOUNCE_MS) {
            stable = raw;
            if (stable) {
                press_start = now;
                long_fired = false;
                ESP_LOGI(TAG, "pressed");
            } else {
                const int64_t held_ms = (now - press_start) / 1000;
                ESP_LOGI(TAG, "released after %lld ms", (long long)held_ms);
                if (!long_fired) {
                    toggle_recording();
                }
            }
        }

        if (stable && !long_fired && (now - press_start) / 1000 >= IDL1_BTN_LONG_MS) {
            long_fired = true;
            power_off();
        }
    }
}

void idl1_button_release_sleep_holds(void)
{
    const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    gpio_hold_dis(IDL1_PIN_PWR_EN);
    gpio_hold_dis(IDL1_PIN_USER_BTN);
    if (cause == ESP_SLEEP_WAKEUP_EXT1) {
        ESP_LOGI(TAG, "woke from power-off by button press");
    }
}

void idl1_button_start(void)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << IDL1_PIN_USER_BTN,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    if (xTaskCreate(button_task, "button", 3072, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "button task create failed");
    }
}
