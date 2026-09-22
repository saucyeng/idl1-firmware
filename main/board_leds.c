/* See board_leds.h. */

#include "board_leds.h"

#include <stdbool.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "ble_service.h"
#include "session.h"
#include "tla2528.h"
#include "wifi_server.h"

static const char *TAG = "board_leds";

/* Role update period, ms: also the green blink half-period (1 Hz blink). */
#define BOARD_LEDS_TICK_MS 500

static TimerHandle_t s_timer;
static bool s_blink_phase;
static bool s_have_last;
static bool s_last_red, s_last_green;

/* Runs on the FreeRTOS timer task. Reads three state flags and writes
 * GPO_VALUE only when the pattern changes: at most one ~0.1 ms I2C write
 * per tick while green blinks, none while both LEDs are steady (the usual
 * recording case: phone connected, red + green solid). */
static void tick_cb(TimerHandle_t t)
{
    (void)t;
    s_blink_phase = !s_blink_phase;

    const bool linked = idl0_ble_is_connected() || idl0_wifi_state() == IDL0_WIFI_ON;
    const bool red    = idl0_session_is_running();
    const bool green  = linked ? true : s_blink_phase;

    if (s_have_last && red == s_last_red && green == s_last_green) return;
    tla2528_led_set(red, green);
    s_have_last  = true;
    s_last_red   = red;
    s_last_green = green;
}

void idl1_board_leds_start(void)
{
    if (s_timer != NULL) return;
    if (!tla2528_is_up()) {
        ESP_LOGW(TAG, "TLA2528 not up: board LEDs dark, onboard LED only");
        return;
    }
    s_timer = xTimerCreate("board_leds", pdMS_TO_TICKS(BOARD_LEDS_TICK_MS),
                           pdTRUE, NULL, tick_cb);
    if (s_timer == NULL || xTimerStart(s_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "LED role timer failed to start; green stays solid");
        return;
    }
}
