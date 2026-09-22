/* Status LED helper.
 *
 * Wraps the XIAO onboard LED (IDL1_PIN_LED_ONBOARD, GPIO15, active-low —
 * see pins.h). The IDL1 board LEDs are TLA2528 GPOs (tla2528_led_set()).
 * Patterns are 1-byte enums; the caller picks one and the module
 * manages timing internally via a FreeRTOS software timer.
 */

#pragma once

#include <stdbool.h>

typedef enum {
    IDL0_LED_OFF,
    IDL0_LED_ON,
    IDL0_LED_BLINK_SLOW,   /* 1 Hz, 50% duty — idle */
    IDL0_LED_BLINK_FAST,   /* 4 Hz, 50% duty — logging */
} idl0_led_pattern_t;

bool idl0_led_init(void);
void idl0_led_set(idl0_led_pattern_t pattern);
