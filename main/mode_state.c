#include "mode_state.h"

#include "esp_log.h"

static const char *TAG = "mode_state";

static EventGroupHandle_t s_group = NULL;

void idl0_mode_init(void)
{
    /* T17: single, explicit creation point instead of the old
     * check-then-act lazy create in idl0_mode_event_group(), which raced
     * concurrent first callers (lost bits, a leaked group). Called once
     * from app_main before any task/subsystem starts, so there is no
     * concurrent caller to race against. */
    if (s_group == NULL) {
        s_group = xEventGroupCreate();
        if (s_group == NULL) {
            ESP_LOGE(TAG, "xEventGroupCreate failed");
        }
    }
}

EventGroupHandle_t idl0_mode_event_group(void)
{
    /* Review fix: this getter is called every loop iteration by the sensor
     * tasks (gps/imu/hrm _task.c) on the active-logging path — logging here
     * on every NULL call would flood blocking UART logs (hard rule: no new
     * ESP_LOGx reachable while logging). idl0_mode_init() already logs once
     * on the actual creation failure; a degraded-boot NULL is silent here. */
    return s_group;
}

void idl0_mode_set_bits(EventBits_t bits)
{
    EventGroupHandle_t group = idl0_mode_event_group();
    if (group == NULL) {
        return;  /* degraded boot — no group to set bits on; already logged */
    }
    xEventGroupSetBits(group, bits);
}

void idl0_mode_clear_bits(EventBits_t bits)
{
    EventGroupHandle_t group = idl0_mode_event_group();
    if (group == NULL) {
        return;  /* degraded boot — no group to clear bits on; already logged */
    }
    xEventGroupClearBits(group, bits);
}

EventBits_t idl0_mode_get_bits(void)
{
    EventGroupHandle_t group = idl0_mode_event_group();
    if (group == NULL) {
        return 0;  /* degraded boot — report no bits set rather than crash */
    }
    return xEventGroupGetBits(group);
}
