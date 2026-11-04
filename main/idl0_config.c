/* idl0_config.json loader. Reads the JSON from the SD card root,
 * derives the §5.4 IMU channel mask, and computes the §5.1 config CRC32.
 * On any failure, out_config is filled with safe defaults so a session
 * can still start. See IDL0_SPEC.md §8 (spec location: docs/README.md).
 */

#include "idl0_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"

#include "idl0_format.h"
#include "idl1_format.h"
#include "imu_ranges.h"
#include "sd_logger.h"
#include "ubx.h"

static const char *TAG = "config";

#define CONFIG_MAX_BYTES 8192

/* Off-list gate for the §8 fields with a fixed hardware value set (the
 * LSM6DSV320X ODR / full-scale ladders, imu_ranges.h). decisions.md P2/P3:
 * an off-list value is rejected — logged once here, per field, per load —
 * and the value already in the struct (the default, or the top-level seed
 * for a per-IMU field) is kept. Never remapped to a nearest neighbour.
 * Returns `valid`. */
static bool accept_or_reject(const char *field, int32_t value, bool valid)
{
    if (!valid) {
        ESP_LOGW(TAG, "%s: %ld is not supported by the LSM6DSV320X — rejected, "
                      "keeping the previous value", field, (long)value);
    }
    return valid;
}

/* Self-heal (F10): replace a rejected number in the parsed tree with the
 * value actually in use, and note that the file needs writing back. After
 * the write, the app's config read (which returns the file verbatim) shows
 * what the logger really runs, and the next load rejects nothing. */
static void heal_number(const cJSON *item, double in_use, bool *healed)
{
    cJSON_SetNumberValue((cJSON *)item, in_use);
    *healed = true;
}

/* Clamp `value` to [lo, hi] (inclusive). Used for §8 fields the spec leaves
 * a free range rather than a fixed hardware set. Logs field/given/applied
 * on any change. */
static int32_t clamp_to_range(const char *field, int32_t value,
                               int32_t lo, int32_t hi)
{
    int32_t clamped = (value < lo) ? lo : (value > hi ? hi : value);
    if (clamped != value) {
        ESP_LOGW(TAG, "%s: invalid value %ld, clamped to %ld",
                 field, (long)value, (long)clamped);
    }
    return clamped;
}

/* All-IMU, all-axis mask (bits 0-17). */
#define DEFAULT_IMU_MASK 0x0003FFFFu

static void fill_defaults(idl0_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    snprintf(cfg->bike_name, sizeof(cfg->bike_name), "%s", "Unconfigured");
    cfg->is_full_suspension   = true;
    cfg->imu_count            = 3;
    cfg->imu_sample_rate_hz   = IDL1_IMU_DEFAULT_ODR_HZ;
    cfg->accel_range_g[0]     = IDL1_IMU_DEFAULT_ACCEL_RANGE_G;
    cfg->accel_range_g[1]     = IDL1_IMU_DEFAULT_ACCEL_RANGE_G;
    cfg->accel_range_g[2]     = IDL1_IMU_DEFAULT_ACCEL_RANGE_G;
    cfg->gyro_range_dps[0]    = IDL1_IMU_DEFAULT_GYRO_RANGE_DPS;
    cfg->gyro_range_dps[1]    = IDL1_IMU_DEFAULT_GYRO_RANGE_DPS;
    cfg->gyro_range_dps[2]    = IDL1_IMU_DEFAULT_GYRO_RANGE_DPS;
    cfg->high_performance_mode = true;
    cfg->imu_enabled[0] = cfg->imu_enabled[1] = cfg->imu_enabled[2] = true;
    cfg->hg_sample_rate_hz    = IDL1_IMU_DEFAULT_HG_ODR_HZ;
    cfg->hg_range_g           = IDL1_IMU_DEFAULT_HG_RANGE_G;
    /* GPS: the SAM-M10Q's factory values (spec §8 "SAM-M10Q (IDL1)"),
     * except the rate: 4 Hz, the most the factory four constellations
     * hold (data sheet UBX-22013293 Table 1). */
    cfg->gps.rate_hz                  = 4;
    cfg->gps.signals                  = IDL0_GPS_SIG_MAJOR | IDL0_GPS_SIG_QZSS | IDL0_GPS_SIG_SBAS;
    cfg->gps.dynamic_model            = UBX_DYNMODEL_PORTABLE;
    cfg->gps.min_elevation_deg        = 5;
    cfg->gps.min_cno_dbhz             = 6;
    cfg->gps.min_satellites           = 3;
    cfg->gps.max_satellites           = 32;
    cfg->gps.fix_gate_satellites      = 0;
    cfg->gps.fix_gate_cno_dbhz        = 0;
    cfg->gps.pdop_mask_x10            = 250;
    cfg->gps.position_accuracy_mask_m = 100;
    cfg->gps.high_precision_nmea      = false;
    cfg->gps.nmea_extra               = 0;
    cfg->imu_channel_mask     = DEFAULT_IMU_MASK;
    cfg->config_crc32         = 0;
    /* HRM disabled by default; channel IDs match spec §5.2 so status.c
     * and session.c can reference them even with no strap configured. */
    cfg->hrm.hr_channel_id    = IDL1_CH_HR_BPM;   /* 36 (F12) */
    cfg->hrm.rr_channel_id    = IDL1_CH_HR_RR;    /* 37 */
    /* Analog + digital channel arrays default empty — users add entries
     * via the app's `+ Add channel…` flow as hardware lands. */
    cfg->analog_sample_rate_hz = 100;
    cfg->analog_channel_count  = 0;
    cfg->digital_channel_count = 0;
}

/* Parse `analog.channels[]` from §8 — user-added ADC channels. Empty / missing
 * array is valid (no analog channels). */
static void load_analog(const cJSON *root, idl0_config_t *cfg)
{
    const cJSON *analog = cJSON_GetObjectItemCaseSensitive(root, "analog");
    if (!cJSON_IsObject(analog)) return;

    const cJSON *rate = cJSON_GetObjectItemCaseSensitive(analog, "sample_rate_hz");
    if (cJSON_IsNumber(rate)) {
        /* Spec §8 leaves analog.sample_rate_hz "not yet defined"; bound it to
         * a sane range (1-1000 Hz) rather than accepting anything, since a
         * negative or huge value flows straight into the ADC scheduler. */
        cfg->analog_sample_rate_hz = (uint16_t)clamp_to_range(
            "analog.sample_rate_hz", rate->valueint, 1, 1000);
    }

    const cJSON *channels = cJSON_GetObjectItemCaseSensitive(analog, "channels");
    if (!cJSON_IsArray(channels)) return;

    cfg->analog_channel_count = 0;
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, channels) {
        if (!cJSON_IsObject(entry)) continue;
        if (cfg->analog_channel_count >= ANALOG_MAX_CHANNELS) {
            ESP_LOGW(TAG, "analog.channels[] truncated at %d entries", ANALOG_MAX_CHANNELS);
            break;
        }
        analog_channel_t *a = &cfg->analog_channels[cfg->analog_channel_count];
        memset(a, 0, sizeof(*a));

        const cJSON *en   = cJSON_GetObjectItemCaseSensitive(entry, "enabled");
        const cJSON *pin  = cJSON_GetObjectItemCaseSensitive(entry, "adc_pin");
        const cJSON *lbl  = cJSON_GetObjectItemCaseSensitive(entry, "label");
        const cJSON *unit = cJSON_GetObjectItemCaseSensitive(entry, "units");
        const cJSON *scl  = cJSON_GetObjectItemCaseSensitive(entry, "scale");
        const cJSON *off  = cJSON_GetObjectItemCaseSensitive(entry, "offset");

        a->enabled = cJSON_IsTrue(en);
        a->adc_pin = cJSON_IsNumber(pin) ? pin->valueint : 0;
        if (cJSON_IsString(lbl) && lbl->valuestring != NULL) {
            strncpy(a->label, lbl->valuestring, sizeof(a->label) - 1);
        }
        if (cJSON_IsString(unit) && unit->valuestring != NULL) {
            strncpy(a->units, unit->valuestring, sizeof(a->units) - 1);
        }
        a->scale  = cJSON_IsNumber(scl) ? (float)scl->valuedouble : 1.0f;
        a->offset = cJSON_IsNumber(off) ? (float)off->valuedouble : 0.0f;

        cfg->analog_channel_count++;
    }
}

/* Parse `digital.channels[]` from §8 — user-added GPIO inputs. Spec 1 ships
 * `kind=marker` only; other kinds are silently skipped here (forward-compat). */
static void load_digital(const cJSON *root, idl0_config_t *cfg)
{
    const cJSON *digital = cJSON_GetObjectItemCaseSensitive(root, "digital");
    if (!cJSON_IsObject(digital)) return;

    const cJSON *channels = cJSON_GetObjectItemCaseSensitive(digital, "channels");
    if (!cJSON_IsArray(channels)) return;

    cfg->digital_channel_count = 0;
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, channels) {
        if (!cJSON_IsObject(entry)) continue;
        if (cfg->digital_channel_count >= DIGITAL_MAX_CHANNELS) {
            ESP_LOGW(TAG, "digital.channels[] truncated at %d entries", DIGITAL_MAX_CHANNELS);
            break;
        }

        const cJSON *kind = cJSON_GetObjectItemCaseSensitive(entry, "kind");
        const char *kind_str = (cJSON_IsString(kind) && kind->valuestring) ?
                               kind->valuestring : "marker";
        /* Spec 1 implements marker only. */
        if (strcmp(kind_str, "marker") != 0) {
            ESP_LOGW(TAG, "digital.channels[] kind=%s not yet supported, skipped",
                     kind_str);
            continue;
        }

        digital_channel_cfg_t *d = &cfg->digital_channels[cfg->digital_channel_count];
        memset(d, 0, sizeof(*d));

        const cJSON *en    = cJSON_GetObjectItemCaseSensitive(entry, "enabled");
        const cJSON *pin   = cJSON_GetObjectItemCaseSensitive(entry, "gpio_pin");
        const cJSON *act   = cJSON_GetObjectItemCaseSensitive(entry, "active_low");
        const cJSON *db    = cJSON_GetObjectItemCaseSensitive(entry, "debounce_ms");
        const cJSON *lbl   = cJSON_GetObjectItemCaseSensitive(entry, "label");

        d->enabled    = cJSON_IsTrue(en);
        d->gpio_pin   = cJSON_IsNumber(pin) ? pin->valueint : -1;
        d->active_low = cJSON_IsBool(act) ? cJSON_IsTrue(act) : true;
        /* debounce_ms (ms): bound to [0, 1000] — a negative value would wrap
         * to a huge uint16_t and effectively disable the marker input. */
        d->debounce_ms = cJSON_IsNumber(db) ?
            (uint16_t)clamp_to_range("digital.channels[].debounce_ms",
                                      db->valueint, 0, 1000) : 20;
        if (cJSON_IsString(lbl) && lbl->valuestring != NULL) {
            strncpy(d->label, lbl->valuestring, sizeof(d->label) - 1);
        } else {
            snprintf(d->label, sizeof(d->label), "Marker");
        }

        cfg->digital_channel_count++;
    }
}

/* Parses "AA:BB:CC:DD:EE:FF" into a 6-byte address in NimBLE wire order
 * (LSB first). Returns true on success. Lenient on case; rejects strings
 * that don't have exactly 17 characters or a non-hex byte. */
static bool parse_hex_addr_le(const char *s, uint8_t *out)
{
    if (s == NULL) return false;
    size_t len = strlen(s);
    if (len < 17) return false;
    uint8_t big[6];
    for (int i = 0; i < 6; i++) {
        unsigned int b = 0;
        if (sscanf(s + i * 3, "%2x", &b) != 1) return false;
        big[i] = (uint8_t)b;
    }
    /* NimBLE addresses are LSB-first; reverse so memcmp against
     * event->disc.addr.val matches directly. */
    for (int i = 0; i < 6; i++) out[i] = big[5 - i];
    return true;
}

/* Parse `wheel_speed.front/rear.enabled` (§8). A missing block or slot
 * leaves that wheel off. */
static void load_wheel_speed(const cJSON *root, idl0_config_t *cfg)
{
    const cJSON *ws = cJSON_GetObjectItemCaseSensitive(root, "wheel_speed");
    if (!cJSON_IsObject(ws)) return;
    static const char *const slot_keys[2] = { "front", "rear" };
    for (int i = 0; i < 2; i++) {
        const cJSON *slot = cJSON_GetObjectItemCaseSensitive(ws, slot_keys[i]);
        if (!cJSON_IsObject(slot)) continue;
        const cJSON *en = cJSON_GetObjectItemCaseSensitive(slot, "enabled");
        cfg->wheel_enabled[i] = cJSON_IsTrue(en);
    }
}

/* One §8 `gps` integer field: kept when it is an integer in [lo, hi],
 * otherwise rejected and logged, leaving the default (spec §8 "SAM-M10Q
 * (IDL1)": never remapped). Returns true when `*out` was set. */
static bool gps_int_field(const cJSON *gps, const char *name, int lo, int hi, int *out)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(gps, name);
    if (item == NULL) return false;
    if (!cJSON_IsNumber(item) || item->valuedouble != (double)item->valueint ||
        item->valueint < lo || item->valueint > hi) {
        ESP_LOGW(TAG, "gps.%s: not an integer in %d..%d — rejected, keeping the default",
                 name, lo, hi);
        return false;
    }
    *out = item->valueint;
    return true;
}

/* Sets or clears `bit` in `*bits` from a §8 `gps` bool field. */
static void gps_bool_bit(const cJSON *gps, const char *name, uint8_t bit, uint8_t *bits)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(gps, name);
    if (!cJSON_IsBool(item)) return;
    if (cJSON_IsTrue(item)) *bits |= bit; else *bits &= (uint8_t)~bit;
}

/* §8 `gps` (spec §8 "SAM-M10Q (IDL1)"). Each field is checked on its own;
 * a rejected one keeps its default. The constellation rate limit is the
 * app's to enforce — the firmware applies the rate it is given. */
static void load_gps(const cJSON *root, idl0_config_t *cfg)
{
    const cJSON *gps = cJSON_GetObjectItemCaseSensitive(root, "gps");
    if (!cJSON_IsObject(gps)) return;
    idl0_gps_settings_t *g = &cfg->gps;
    int v;

    if (gps_int_field(gps, "sample_rate_hz", 1, 18, &v)) g->rate_hz = (uint8_t)v;

    static const struct { const char *name; uint8_t bit; } constellations[] = {
        { "GPS", IDL0_GPS_SIG_GPS }, { "GALILEO", IDL0_GPS_SIG_GALILEO },
        { "BEIDOU", IDL0_GPS_SIG_BEIDOU }, { "GLONASS", IDL0_GPS_SIG_GLONASS },
    };
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(gps, "constellations");
    if (list != NULL) {
        uint8_t major = 0;
        bool ok = cJSON_IsArray(list);
        if (ok) {
            const cJSON *s;
            cJSON_ArrayForEach(s, list) {
                bool known = false;
                for (size_t i = 0; cJSON_IsString(s) && i < sizeof(constellations) / sizeof(constellations[0]); i++) {
                    if (strcmp(s->valuestring, constellations[i].name) == 0) {
                        major |= constellations[i].bit;
                        known = true;
                    }
                }
                if (!known) ok = false;
            }
        }
        if (ok && major != 0) {
            g->signals = (uint8_t)((g->signals & ~IDL0_GPS_SIG_MAJOR) | major);
        } else {
            ESP_LOGW(TAG, "gps.constellations: not a non-empty list of GPS/GALILEO/BEIDOU/GLONASS "
                          "— rejected, keeping all four");
        }
    }
    gps_bool_bit(gps, "qzss_enabled", IDL0_GPS_SIG_QZSS, &g->signals);
    gps_bool_bit(gps, "sbas_enabled", IDL0_GPS_SIG_SBAS, &g->signals);

    /* "airborne" is the <1 g model, the gentlest of the three. */
    static const struct { const char *name; uint8_t model; } models[] = {
        { "portable",    UBX_DYNMODEL_PORTABLE },
        { "stationary",  UBX_DYNMODEL_STATIONARY },
        { "pedestrian",  UBX_DYNMODEL_PEDESTRIAN },
        { "automotive",  UBX_DYNMODEL_AUTOMOTIVE },
        { "sea",         UBX_DYNMODEL_SEA },
        { "airborne",    UBX_DYNMODEL_AIRBORNE_1G },
        { "airborne_2g", UBX_DYNMODEL_AIRBORNE_2G },
        { "airborne_4g", UBX_DYNMODEL_AIRBORNE_4G },
    };
    const cJSON *dm = cJSON_GetObjectItemCaseSensitive(gps, "dynamic_model");
    if (dm != NULL) {
        bool known = false;
        for (size_t i = 0; cJSON_IsString(dm) && i < sizeof(models) / sizeof(models[0]); i++) {
            if (strcmp(dm->valuestring, models[i].name) == 0) {
                g->dynamic_model = models[i].model;
                known = true;
            }
        }
        if (!known) {
            ESP_LOGW(TAG, "gps.dynamic_model: unknown — rejected, keeping the default");
        }
    }

    if (gps_int_field(gps, "min_elevation_deg", 0, 60, &v))   g->min_elevation_deg = (int8_t)v;
    if (gps_int_field(gps, "min_cno_dbhz", 0, 50, &v))        g->min_cno_dbhz = (uint8_t)v;
    if (gps_int_field(gps, "max_satellites", 1, 32, &v))      g->max_satellites = (uint8_t)v;
    if (gps_int_field(gps, "min_satellites", 1, 32, &v)) {
        if (v <= g->max_satellites) {
            g->min_satellites = (uint8_t)v;
        } else {
            ESP_LOGW(TAG, "gps.min_satellites: %d is above max_satellites %u — rejected, "
                          "keeping the default", v, (unsigned)g->max_satellites);
        }
    }
    if (g->min_satellites > g->max_satellites) {
        /* A max below the default min: keep the pair consistent. */
        ESP_LOGW(TAG, "gps.max_satellites: below min_satellites — rejected, keeping the default");
        g->max_satellites = 32;
    }
    if (gps_int_field(gps, "fix_gate_satellites", 0, 32, &v)) g->fix_gate_satellites = (uint8_t)v;
    if (gps_int_field(gps, "fix_gate_cno_dbhz", 0, 50, &v))   g->fix_gate_cno_dbhz = (uint8_t)v;
    if (gps_int_field(gps, "position_accuracy_mask_m", 1, 100, &v)) {
        g->position_accuracy_mask_m = (uint16_t)v;
    }

    /* PDOP mask: a number with one decimal, 1.0..25.0, sent x 10. */
    const cJSON *pdop = cJSON_GetObjectItemCaseSensitive(gps, "pdop_mask");
    if (pdop != NULL) {
        const double x10 = cJSON_IsNumber(pdop) ? pdop->valuedouble * 10.0 : -1.0;
        const long rounded = (long)(x10 + 0.5);
        if (x10 >= 10.0 - 1e-6 && x10 <= 250.0 + 1e-6 &&
            (x10 - (double)rounded) < 1e-6 && ((double)rounded - x10) < 1e-6) {
            g->pdop_mask_x10 = (uint16_t)rounded;
        } else {
            ESP_LOGW(TAG, "gps.pdop_mask: not 1.0..25.0 in steps of 0.1 — rejected, "
                          "keeping the default");
        }
    }

    const cJSON *hp = cJSON_GetObjectItemCaseSensitive(gps, "high_precision_nmea");
    if (cJSON_IsBool(hp)) g->high_precision_nmea = cJSON_IsTrue(hp);

    static const struct { const char *name; uint8_t bit; } sentences[] = {
        { "GLL", IDL0_GPS_NMEA_GLL }, { "GSA", IDL0_GPS_NMEA_GSA },
        { "GSV", IDL0_GPS_NMEA_GSV }, { "VTG", IDL0_GPS_NMEA_VTG },
        { "ZDA", IDL0_GPS_NMEA_ZDA },
    };
    const cJSON *nmea = cJSON_GetObjectItemCaseSensitive(gps, "nmea_sentences");
    if (cJSON_IsArray(nmea)) {
        g->nmea_extra = 0;
        const cJSON *s;
        cJSON_ArrayForEach(s, nmea) {
            if (!cJSON_IsString(s)) continue;
            if (strcmp(s->valuestring, "RMC") == 0 || strcmp(s->valuestring, "GGA") == 0) {
                continue;  /* always on */
            }
            bool known = false;
            for (size_t i = 0; i < sizeof(sentences) / sizeof(sentences[0]); i++) {
                if (strcmp(s->valuestring, sentences[i].name) == 0) {
                    g->nmea_extra |= sentences[i].bit;
                    known = true;
                }
            }
            if (!known) {
                ESP_LOGW(TAG, "gps.nmea_sentences: \"%s\" is unknown — skipped",
                         s->valuestring);
            }
        }
    }
}

static void load_hrm(const cJSON *root, idl0_config_t *cfg)
{
    /* Default: disabled, channel IDs pinned to spec (§5.2). */
    memset(&cfg->hrm, 0, sizeof(cfg->hrm));
    cfg->hrm.hr_channel_id = IDL1_CH_HR_BPM;
    cfg->hrm.rr_channel_id = IDL1_CH_HR_RR;

    const cJSON *hrm = cJSON_GetObjectItemCaseSensitive(root, "heart_rate_monitor");
    if (!cJSON_IsObject(hrm)) return;

    const cJSON *en = cJSON_GetObjectItemCaseSensitive(hrm, "enabled");
    cfg->hrm.enabled = cJSON_IsTrue(en);

    const cJSON *addr = cJSON_GetObjectItemCaseSensitive(hrm, "device_address");
    if (cJSON_IsString(addr) && addr->valuestring != NULL) {
        if (!parse_hex_addr_le(addr->valuestring, cfg->hrm.address)) {
            /* Address malformed — keep enabled=false so we don't scan
             * for an unknown peer. */
            cfg->hrm.enabled = false;
        }
    } else if (cfg->hrm.enabled) {
        /* Enabled but no address — can't connect to anything. */
        cfg->hrm.enabled = false;
    }

    const cJSON *name = cJSON_GetObjectItemCaseSensitive(hrm, "device_name");
    if (cJSON_IsString(name) && name->valuestring != NULL) {
        strncpy(cfg->hrm.name, name->valuestring, sizeof(cfg->hrm.name) - 1);
    }
}

/* Sets the 6 mask bits for one IMU from a channels.imuN JSON object. */
static uint32_t parse_imu_axes(const cJSON *imu_obj, int imu_index)
{
    static const char *axis_keys[6] = {
        "accel_x", "accel_y", "accel_z", "gyro_x", "gyro_y", "gyro_z"
    };
    uint32_t bits = 0;
    for (int axis = 0; axis < 6; axis++) {
        const cJSON *v = cJSON_GetObjectItemCaseSensitive(imu_obj, axis_keys[axis]);
        if (cJSON_IsTrue(v)) {
            bits |= (1u << axis);
        }
    }
    return bits << (imu_index * 6);
}

/* Linear pre-scan of raw JSON text for excessive `[`/`{` nesting depth,
 * before handing the buffer to cJSON's recursive-descent parser. Real §8
 * config nests <= 5 deep; 16 is generous headroom. cJSON has its own depth
 * limit (1000), but that check only trips after the call stack is already
 * deep enough to overflow the 8 KB NimBLE host task stack — this catches
 * pathological input (e.g. ~1000 nested brackets, ~2 KB) before any
 * recursion happens. Correctly skips over string literals (quote and
 * backslash-escape handling) so brackets inside string values don't count. */
#define JSON_MAX_NESTING_DEPTH 16

static bool json_depth_ok(const char *buf, size_t len)
{
    int depth = 0;
    bool in_string = false;
    bool escaped = false;

    for (size_t i = 0; i < len; i++) {
        char c = buf[i];
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }
        if (c == '"') {
            in_string = true;
        } else if (c == '[' || c == '{') {
            depth++;
            if (depth > JSON_MAX_NESTING_DEPTH) {
                return false;
            }
        } else if (c == ']' || c == '}') {
            if (depth > 0) depth--;
        }
    }
    return true;
}

bool idl0_config_load(idl0_config_t *out_config)
{
    fill_defaults(out_config);

    FILE *f = fopen(IDL0_CONFIG_PATH, "rb");
    if (f == NULL) {
        /* Write side is remove(live) then rename(tmp, live) — not atomic
         * (FAT32 rename() can't overwrite an existing target, hence the
         * remove-first dance in idl0_config_write_json()). Power loss
         * between those two calls leaves NO live file even though the
         * complete .tmp is still on disk, so a boot right after would
         * silently fall back to defaults. Recover the .tmp here first. */
        static const char *tmp_path = IDL0_CONFIG_PATH ".tmp";
        FILE *tmp_check = fopen(tmp_path, "rb");
        if (tmp_check != NULL) {
            fclose(tmp_check);
            if (rename(tmp_path, IDL0_CONFIG_PATH) == 0) {
                ESP_LOGW(TAG, "recovered config from interrupted replace");
                f = fopen(IDL0_CONFIG_PATH, "rb");
            }
        }
        if (f == NULL) {
            ESP_LOGW(TAG, "%s not found — using defaults", IDL0_CONFIG_PATH);
            return false;
        }
    }

    uint8_t *raw = malloc(CONFIG_MAX_BYTES);
    if (raw == NULL) {
        ESP_LOGE(TAG, "out of memory reading config");
        fclose(f);
        return false;
    }
    size_t len = fread(raw, 1, CONFIG_MAX_BYTES, f);
    fclose(f);

    /* CRC32 over the exact on-disk bytes — before any parsing (§5.1). */
    out_config->config_crc32 = idl0_format_crc32(raw, len);

    /* Reject pathological nesting before it ever reaches cJSON's recursive
     * descent parser — see json_depth_ok() above. Treated the same as a
     * malformed-JSON file: defaults are already loaded. */
    if (!json_depth_ok((const char *)raw, len)) {
        free(raw);
        ESP_LOGW(TAG, "config JSON nesting too deep — using defaults");
        return false;
    }

    cJSON *root = cJSON_ParseWithLength((const char *)raw, len);
    free(raw);
    if (root == NULL) {
        ESP_LOGW(TAG, "config JSON malformed — using defaults");
        return false;
    }

    const cJSON *bike = cJSON_GetObjectItemCaseSensitive(root, "bike_profile");
    if (cJSON_IsObject(bike)) {
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(bike, "name");
        if (cJSON_IsString(name) && name->valuestring != NULL) {
            snprintf(out_config->bike_name, sizeof(out_config->bike_name),
                     "%s", name->valuestring);
        }
        const cJSON *cnt = cJSON_GetObjectItemCaseSensitive(bike, "imu_count");
        if (cJSON_IsNumber(cnt)) {
            int c = cnt->valueint;
            out_config->imu_count = (uint8_t)(c < 1 ? 1 : (c > 3 ? 3 : c));
        }
    }

    /* Set when a rejected field was replaced in `root` (F10). */
    bool healed = false;

    const cJSON *imu = cJSON_GetObjectItemCaseSensitive(root, "imu");
    if (cJSON_IsObject(imu)) {
        const cJSON *rate = cJSON_GetObjectItemCaseSensitive(imu, "sample_rate_hz");
        if (cJSON_IsNumber(rate)) {
            if (accept_or_reject("imu.sample_rate_hz", rate->valueint,
                                 idl1_imu_odr_valid(rate->valueint))) {
                out_config->imu_sample_rate_hz = (uint16_t)rate->valueint;
            } else {
                heal_number(rate, out_config->imu_sample_rate_hz, &healed);
            }
        }

        /* Top-level accel_range_g / gyro_range_dps seed all three slots for
         * back-compat with config files that lack per-IMU sub-blocks. A
         * rejected seed is healed after the per-IMU blocks, to imu0's value
         * in use (the same rule the app's Send applies). */
        const cJSON *arange_top = cJSON_GetObjectItemCaseSensitive(imu, "accel_range_g");
        bool heal_arange_top = false;
        if (cJSON_IsNumber(arange_top)) {
            if (accept_or_reject("imu.accel_range_g", arange_top->valueint,
                                 idl1_imu_accel_range_valid(arange_top->valueint))) {
                const uint8_t v = (uint8_t)arange_top->valueint;
                out_config->accel_range_g[0] = v;
                out_config->accel_range_g[1] = v;
                out_config->accel_range_g[2] = v;
            } else {
                heal_arange_top = true;
            }
        }
        const cJSON *grange_top = cJSON_GetObjectItemCaseSensitive(imu, "gyro_range_dps");
        bool heal_grange_top = false;
        if (cJSON_IsNumber(grange_top)) {
            if (accept_or_reject("imu.gyro_range_dps", grange_top->valueint,
                                 idl1_imu_gyro_range_valid(grange_top->valueint))) {
                const uint16_t v = (uint16_t)grange_top->valueint;
                out_config->gyro_range_dps[0] = v;
                out_config->gyro_range_dps[1] = v;
                out_config->gyro_range_dps[2] = v;
            } else {
                heal_grange_top = true;
            }
        }

        /* High-g range and rate, shared by every IMU (F11). */
        const cJSON *hg_range = cJSON_GetObjectItemCaseSensitive(imu, "hg_range_g");
        if (cJSON_IsNumber(hg_range)) {
            if (accept_or_reject("imu.hg_range_g", hg_range->valueint,
                                 idl1_imu_hg_range_valid(hg_range->valueint))) {
                out_config->hg_range_g = (uint16_t)hg_range->valueint;
            } else {
                heal_number(hg_range, out_config->hg_range_g, &healed);
            }
        }
        const cJSON *hg_rate = cJSON_GetObjectItemCaseSensitive(imu, "hg_sample_rate_hz");
        if (cJSON_IsNumber(hg_rate)) {
            if (accept_or_reject("imu.hg_sample_rate_hz", hg_rate->valueint,
                                 idl1_imu_hg_odr_valid(hg_rate->valueint))) {
                out_config->hg_sample_rate_hz = (uint16_t)hg_rate->valueint;
            } else {
                heal_number(hg_rate, out_config->hg_sample_rate_hz, &healed);
            }
        }

        const cJSON *hp = cJSON_GetObjectItemCaseSensitive(imu, "high_performance_mode");
        if (cJSON_IsBool(hp)) {
            out_config->high_performance_mode = cJSON_IsTrue(hp);
        }
        const cJSON *lp = cJSON_GetObjectItemCaseSensitive(imu, "low_power_mode");
        if (cJSON_IsBool(lp)) {
            out_config->low_power_mode = cJSON_IsTrue(lp);
        }

        /* Per-IMU sub-blocks: imu.imu0, imu.imu1, imu.imu2.
         * Each carries enabled, accel_range_g, gyro_range_dps, and channels.
         * Per-IMU values override the top-level seed parsed above. */
        static const char *imu_keys[3] = { "imu0", "imu1", "imu2" };
        uint32_t mask = 0;
        bool have_per_imu_channels = false;
        for (int i = 0; i < 3; i++) {
            const cJSON *imu_blk =
                cJSON_GetObjectItemCaseSensitive(imu, imu_keys[i]);
            if (!cJSON_IsObject(imu_blk)) continue;

            const cJSON *en = cJSON_GetObjectItemCaseSensitive(imu_blk, "enabled");
            if (cJSON_IsBool(en)) {
                out_config->imu_enabled[i] = cJSON_IsTrue(en);
            }
            const cJSON *hg_en = cJSON_GetObjectItemCaseSensitive(imu_blk, "hg_enabled");
            if (cJSON_IsBool(hg_en)) {
                out_config->hg_enabled[i] = cJSON_IsTrue(hg_en);
            }
            const cJSON *ar = cJSON_GetObjectItemCaseSensitive(imu_blk, "accel_range_g");
            if (cJSON_IsNumber(ar)) {
                if (accept_or_reject("imu.imuN.accel_range_g", ar->valueint,
                                     idl1_imu_accel_range_valid(ar->valueint))) {
                    out_config->accel_range_g[i] = (uint8_t)ar->valueint;
                } else {
                    heal_number(ar, out_config->accel_range_g[i], &healed);
                }
            }
            const cJSON *gr = cJSON_GetObjectItemCaseSensitive(imu_blk, "gyro_range_dps");
            if (cJSON_IsNumber(gr)) {
                if (accept_or_reject("imu.imuN.gyro_range_dps", gr->valueint,
                                     idl1_imu_gyro_range_valid(gr->valueint))) {
                    out_config->gyro_range_dps[i] = (uint16_t)gr->valueint;
                } else {
                    heal_number(gr, out_config->gyro_range_dps[i], &healed);
                }
            }
            const cJSON *channels_blk =
                cJSON_GetObjectItemCaseSensitive(imu_blk, "channels");
            if (cJSON_IsObject(channels_blk)) {
                mask |= parse_imu_axes(channels_blk, i);
                have_per_imu_channels = true;
            }
        }
        if (have_per_imu_channels) {
            out_config->imu_channel_mask = mask;
        }
        if (heal_arange_top) heal_number(arange_top, out_config->accel_range_g[0], &healed);
        if (heal_grange_top) heal_number(grange_top, out_config->gyro_range_dps[0], &healed);

        /* Back-compat: old schema had imu.channels.imuN (flat channels block).
         * Only falls back if no per-IMU sub-blocks provided a channels object. */
        if (!have_per_imu_channels) {
            const cJSON *channels = cJSON_GetObjectItemCaseSensitive(imu, "channels");
            if (cJSON_IsObject(channels)) {
                uint32_t legacy_mask = 0;
                for (int i = 0; i < 3; i++) {
                    const cJSON *obj =
                        cJSON_GetObjectItemCaseSensitive(channels, imu_keys[i]);
                    if (cJSON_IsObject(obj)) {
                        legacy_mask |= parse_imu_axes(obj, i);
                    }
                }
                out_config->imu_channel_mask = legacy_mask;
            }
        }
    }

    load_hrm(root, out_config);
    load_analog(root, out_config);
    load_digital(root, out_config);
    load_wheel_speed(root, out_config);
    load_gps(root, out_config);

    /* F10: write the healed tree back, through the same tmp+rename path a
     * config push uses, and take the CRC from the new bytes so this boot's
     * sessions name the file as it now is (§5.1). A failed write only
     * leaves the stale file in place; the values in use are unaffected. */
    if (healed) {
        char *text = cJSON_PrintUnformatted(root);
        if (text != NULL) {
            const size_t text_len = strlen(text);
            if (idl0_config_write_json(text, text_len) == IDL0_CONFIG_WRITE_OK) {
                out_config->config_crc32 = idl0_format_crc32((const uint8_t *)text, text_len);
                ESP_LOGW(TAG, "config: rejected values replaced with the ones in use; file rewritten");
            } else {
                ESP_LOGW(TAG, "config: could not rewrite the healed file");
            }
            cJSON_free(text);
        }
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "config loaded: bike '%s', %u IMUs, mask 0x%05lX, crc 0x%08lX",
             out_config->bike_name, out_config->imu_count,
             (unsigned long)out_config->imu_channel_mask,
             (unsigned long)out_config->config_crc32);
    return true;
}

bool idl0_config_save(const idl0_config_t *config)
{
    /* Config push is implemented with the WiFi server in P8. The firmware
     * does not write idl0_config.json during P5-P7. */
    (void)config;
    ESP_LOGW(TAG, "idl0_config_save not implemented until P8");
    return false;
}

idl0_config_write_result_t idl0_config_write_json(const char *buf, size_t len)
{
    if (buf == NULL || len == 0 || len > CONFIG_MAX_BYTES) {
        return IDL0_CONFIG_WRITE_BAD_JSON;
    }

    /* Reject pathological nesting before it ever reaches cJSON's recursive
     * descent parser — see json_depth_ok() above. `buf` comes from the BLE
     * config-push path, which runs on the 8 KB NimBLE host task stack. */
    if (!json_depth_ok(buf, len)) {
        return IDL0_CONFIG_WRITE_BAD_JSON;
    }

    /* Validate it parses as JSON. We don't enforce the §8 schema here — the
     * app's config editor is the gatekeeper for shape; firmware just refuses
     * outright garbage so a corrupt push can't replace a good config. */
    cJSON *root = cJSON_ParseWithLength(buf, len);
    if (root == NULL) {
        return IDL0_CONFIG_WRITE_BAD_JSON;
    }
    cJSON_Delete(root);

    /* Persist verbatim so the on-disk bytes drive config_crc32 (§5.1). Write
     * to a temp path and rename on success so a short or interrupted write
     * can't corrupt the live config. */
    static const char *tmp_path = IDL0_CONFIG_PATH ".tmp";
    FILE *f = fopen(tmp_path, "wb");
    if (f == NULL) {
        return IDL0_CONFIG_WRITE_IO_ERROR;
    }
    size_t written = fwrite(buf, 1, len, f);
    int close_err = fclose(f);
    if (written != len || close_err != 0) {
        remove(tmp_path);
        return IDL0_CONFIG_WRITE_IO_ERROR;
    }

    /* FAT32 (ESP-IDF VFS) rename() does NOT overwrite an existing target — it
     * returns -1. Unlink the live config first so second-and-later pushes
     * don't fail. The unlink may legitimately fail when no live config exists
     * yet (first push of a freshly formatted SD); that's fine — only a rename
     * failure is fatal. */
    remove(IDL0_CONFIG_PATH);
    if (rename(tmp_path, IDL0_CONFIG_PATH) != 0) {
        remove(tmp_path);
        return IDL0_CONFIG_WRITE_IO_ERROR;
    }
    return IDL0_CONFIG_WRITE_OK;
}
