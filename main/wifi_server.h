/* WiFi AP mode + HTTP server for on-demand file transfer and config push.
 *
 * SSID derived from the MAC per §3.6 (idl0_device_name → "IDL1-XXXX").
 * Password is the §6 shared default until per-device passwords land
 * (§6 TODO #16). See §6.1 for endpoint contracts. WiFi is off by
 * default; CMD_WIFI_ON (§7.2) starts the AP + HTTP server, CMD_WIFI_OFF
 * stops them.
 */

#pragma once

#include <stdbool.h>

/* WiFi control-protocol version reported by GET /ping (SPEC §6.1).
 * Bump on breaking changes to the HTTP control plane; the app refuses
 * operations on a major mismatch. */
#define IDL0_WIFI_PROTO_VERSION 1

/* WiFi subsystem state, mapped 1:1 onto the §7.3 WiFi: status token. */
typedef enum {
    IDL0_WIFI_OFF = 0,  /* AP not running */
    IDL0_WIFI_ON,       /* AP up + HTTP server listening */
} idl0_wifi_state_t;

idl0_wifi_state_t idl0_wifi_state(void);
const char *idl0_wifi_state_str(idl0_wifi_state_t state);

/* Bring up the AP and start the HTTP server. Idempotent — repeated
 * calls while already up return true without restarting the radio. */
bool idl0_wifi_start(void);

/* Stop the HTTP server and the AP. Idempotent and safe to call from several
 * tasks (serialised internally). A /download or /ota in progress is cut off
 * at its next chunk (F7). Blocks while httpd_stop() joins the httpd worker,
 * so never call it from the NimBLE host task (use idl0_wifi_stop_deferred). */
bool idl0_wifi_stop(void);

/* Defers idl0_wifi_stop() to a one-shot esp_timer (500 ms out) instead of
 * running it on the caller's task — for callers that must not block on
 * httpd_stop() joining the httpd worker task (T11), e.g. a BLE command
 * callback running on the NimBLE host task. Shares its timer with the
 * POST /wifi_off handler and is likewise idempotent: queuing a second
 * deferred stop while one is already pending is a no-op (see
 * arm_wifi_off_timer() in wifi_server.c). */
void idl0_wifi_stop_deferred(void);
