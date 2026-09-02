/* Single source of truth for "what mode is the firmware in".
 * Producers: wifi_server.c (WIFI_UP bit), session.c (LOGGING_ACTIVE bit).
 * Subscribers wait on bit changes via xEventGroupWaitBits / xEventGroupGetBits;
 * no per-subsystem callbacks.
 */
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define IDL0_MODE_BIT_WIFI_UP         BIT0
#define IDL0_MODE_BIT_LOGGING_ACTIVE  BIT1

/* T17: creates the global mode event group. Must be called once from
 * app_main, before any task/subsystem that may call idl0_mode_event_group()
 * or the set/clear/get helpers below, starts. Not safe to call concurrently
 * with itself or with idl0_mode_event_group() — that race (lost bits, a
 * leaked group) is exactly what this replaces the old lazy-create with. */
void idl0_mode_init(void);

/* Returns the global mode event group created by idl0_mode_init(). Safe to
 * call from any task PROVIDED idl0_mode_init() has already run. Does not
 * create the group itself; if idl0_mode_init() was never called (or failed),
 * logs an error and returns NULL — callers already tolerate a NULL/no-op
 * mode group during degraded boot. */
EventGroupHandle_t idl0_mode_event_group(void);

/* Atomically sets the given bits in the global mode event group. */
void idl0_mode_set_bits(EventBits_t bits);

/* Atomically clears the given bits in the global mode event group. */
void idl0_mode_clear_bits(EventBits_t bits);

/* Returns a snapshot of the global mode event group's current bits. */
EventBits_t idl0_mode_get_bits(void);
