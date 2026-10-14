/* See session_files.h. */

#include "session_files.h"

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cJSON.h"

/* Longest file name kept, bytes incl. NUL. Session names are 24 chars
 * (`YYYY-MM-DD_HH-MM-SS.idl0`) or `tmp_<boot_ms>.idl0`; longer names are
 * not ours and are skipped from the newest list (still counted in total). */
#define NAME_MAX_BYTES 64

bool idl0_read_session_id_hex(const char *path, char out[33])
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return false;
    uint8_t prefix[21];  /* 4 magic + 1 schema + 16 uuid (§5.1) */
    size_t got = fread(prefix, 1, sizeof(prefix), f);
    fclose(f);
    if (got < sizeof(prefix)) return false;
    if (memcmp(prefix, "IDL0", 4) != 0) return false;

    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 16; i++) {
        uint8_t b = prefix[5 + i];
        out[i * 2]     = hex[b >> 4];
        out[i * 2 + 1] = hex[b & 0x0F];
    }
    out[32] = '\0';
    return true;
}

/* Inserts [name] into [top], kept sorted in descending byte order and capped
 * at [max] entries; [*count] is the number held. A name not among the
 * largest [max] is dropped. */
static void keep_newest(char top[][NAME_MAX_BYTES], int *count, int max, const char *name)
{
    int pos = *count;
    while (pos > 0 && strcmp(name, top[pos - 1]) > 0) pos--;
    if (pos >= max) return;
    int last = (*count < max) ? *count : max - 1;
    for (int i = last; i > pos; i--) memcpy(top[i], top[i - 1], NAME_MAX_BYTES);
    strncpy(top[pos], name, NAME_MAX_BYTES - 1);
    top[pos][NAME_MAX_BYTES - 1] = '\0';
    if (*count < max) (*count)++;
}

char *idl0_session_files_newest_json(int max, size_t *out_len)
{
    if (max < 0) max = 0;
    char (*top)[NAME_MAX_BYTES] = calloc((size_t)(max > 0 ? max : 1), NAME_MAX_BYTES);
    if (top == NULL) return NULL;
    int held = 0;
    uint32_t total = 0;

    DIR *dir = opendir(IDL0_SESSIONS_DIR);
    if (dir != NULL) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_name[0] == '.') continue;  /* skip "." / ".." */
            total++;
            if (strlen(ent->d_name) >= NAME_MAX_BYTES) continue;
            keep_newest(top, &held, max, ent->d_name);
        }
        closedir(dir);
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *files = cJSON_CreateArray();
    if (root == NULL || files == NULL) {
        cJSON_Delete(root);
        cJSON_Delete(files);
        free(top);
        return NULL;
    }
    cJSON_AddNumberToObject(root, "total", (double)total);
    cJSON_AddItemToObject(root, "files", files);

    for (int i = 0; i < held; i++) {
        char path[96];
        int n = snprintf(path, sizeof(path), "%s/%s", IDL0_SESSIONS_DIR, top[i]);
        if (n < 0 || (size_t)n >= sizeof(path)) continue;
        struct stat st;
        if (stat(path, &st) != 0) continue;
        cJSON *obj = cJSON_CreateObject();
        if (obj == NULL) continue;
        cJSON_AddStringToObject(obj, "name", top[i]);
        cJSON_AddNumberToObject(obj, "size", (double)st.st_size);
        char sid[33];
        if (idl0_read_session_id_hex(path, sid)) {
            cJSON_AddStringToObject(obj, "session_id", sid);
        }
        cJSON_AddItemToArray(files, obj);
    }
    free(top);

    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (body != NULL && out_len != NULL) *out_len = strlen(body);
    return body;
}
