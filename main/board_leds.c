/* See board_leds.h. */

#include "board_leds.h"

#include <stdbool.h>
#include <stdint.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ble_service.h"
#include "session.h"
#include "tla2528.h"
#include "wifi_server.h"

static const char *TAG = "board_leds";

/* Pattern step, ms: the finest timing any pattern needs. */
#define BOARD_LEDS_TICK_MS 100

static TaskHandle_t  s_task;
static volatile bool s_booted;

void idl1_board_leds_set_booted(void)
{
    s_booted = true;
}

/* (red, green) for tick number `n` (one tick = BOARD_LEDS_TICK_MS). */
static void pattern(uint32_t n, bool *red, bool *green)
{
    const bool linked    = idl0_ble_is_connected() || idl0_wifi_state() == IDL0_WIFI_ON;
    const bool recording = idl0_session_is_running();
    const bool slow      = (n / 5) % 2 == 0;            /* 1 Hz, 50 % */

    if (!s_booted) {
        /* Fast alternate: swap every two ticks (200 ms). */
        *red   = (n / 2) % 2 == 0;
        *green = !*red;
        return;
    }

    switch (idl0_wifi_activity()) {
        case IDL0_WIFI_ACT_OTA_VERIFY:
            *red = true;  *green = true;  return;
        case IDL0_WIFI_ACT_OTA_RX:
            *red = slow;  *green = !slow; return;
        case IDL0_WIFI_ACT_TRANSFER:
            *red = false; *green = n % 2 == 0; return;   /* 5 Hz */
        case IDL0_WIFI_ACT_NONE:
        default:
            break;
    }

    *red = recording;
    if (idl0_wifi_state() == IDL0_WIFI_ON) {
        /* Double-blink once a second: on, off, on, then off for 700 ms. */
        const uint32_t k = n % 10;
        *green = (k == 0 || k == 2);
        return;
    }
    *green = linked ? true : slow;
}

static void leds_task(void *arg)
{
    (void)arg;
    bool have_last = false;
    bool last_red = false, last_green = false;
    uint32_t n = 0;
    for (;;) {
        bool red = false, green = false;
        pattern(n++, &red, &green);
        if (!have_last || red != last_red || green != last_green) {
            /* Written only on a change: no bus traffic while the pattern
             * holds (the usual recording case). A failed write leaves
             * `have_last` false so the next tick tries again. */
            have_last = tla2528_led_set(red, green);
            last_red = red;
            last_green = green;
        }
        vTaskDelay(pdMS_TO_TICKS(BOARD_LEDS_TICK_MS));
    }
}

void idl1_board_leds_start(void)
{
    if (s_task != NULL) return;
    if (!tla2528_is_up()) {
        ESP_LOGW(TAG, "TLA2528 not up: board LEDs dark, onboard LED only");
        return;
    }
    /* Priority 3: above the timer service (1), where this used to run and
     * starved past its I2C timeout, below the IMU drain (5). */
    if (xTaskCreate(leds_task, "board_leds", 3072, NULL, 3, &s_task) != pdPASS) {
        ESP_LOGE(TAG, "LED task failed to start; LEDs stay as they are");
        s_task = NULL;
    }
}
