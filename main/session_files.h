/* Session-file listing shared by WiFi GET /files (§6.1) and BLE
 * CMD_FILES_READ_BEGIN (§7.2): the sessions directory, reading a file's
 * session UUID from its header, and the "newest N files" snapshot the BLE
 * command serves. */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "sd_logger.h"

/* Directory holding the session logs (§10.2). */
#define IDL0_SESSIONS_DIR IDL0_SD_MOUNT_POINT "/sessions"

/* Files listed by CMD_FILES_READ_BEGIN (§7.2): the newest ten. Older files
 * are assumed imported or deliberately ignored; WiFi /files lists them all. */
#define IDL0_FILES_READ_MAX 10

/* Reads the 16-byte session UUID from the v3 header of [path] and renders it
 * as 32 lowercase hex chars into [out] (33 bytes incl. NUL). Returns false on
 * a short read or magic mismatch; callers then omit "session_id". */
bool idl0_read_session_id_hex(const char *path, char out[33]);

/* Builds the CMD_FILES_READ_BEGIN payload (§7.2) as compact JSON,
 * {"total":N,"files":[{"name":..,"size":..,"session_id":..}, ...]}, listing
 * at most [max] files, newest first (names in descending byte order). Returns
 * a heap string the caller frees with free(), and its length (bytes, no NUL)
 * in [out_len]; NULL on allocation failure. A missing or empty sessions
 * directory gives {"total":0,"files":[]}. Reads the SD card: never call it
 * while a session is logging. */
char *idl0_session_files_newest_json(int max, size_t *out_len);
