/* u-blox SAM-M10Q GPS driver — UBX configuration at boot, NMEA parser,
 * UART.
 *
 * IDL1 wires both UART directions (IDL0's MAX-M10S had an open TX trace
 * and ran at factory defaults). idl0_gps_init applies the §8 `gps` block
 * to the module over UBX-CFG-VALSET (RAM layer, re-applied every boot),
 * then the GPS task reads RMC + GGA from the same UART.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* `signals` bits: constellations and augmentations enabled (§8
 * `gps.constellations`, `qzss_enabled`, `sbas_enabled`). Same bit order as
 * the .idl1 GPS_CONFIG record. */
#define IDL0_GPS_SIG_GPS      (1u << 0)
#define IDL0_GPS_SIG_GALILEO  (1u << 1)
#define IDL0_GPS_SIG_BEIDOU   (1u << 2)
#define IDL0_GPS_SIG_GLONASS  (1u << 3)
#define IDL0_GPS_SIG_QZSS     (1u << 4)
#define IDL0_GPS_SIG_SBAS     (1u << 5)
#define IDL0_GPS_SIG_MAJOR    (IDL0_GPS_SIG_GPS | IDL0_GPS_SIG_GALILEO | \
                               IDL0_GPS_SIG_BEIDOU | IDL0_GPS_SIG_GLONASS)

/* `nmea_extra` bits: `gps.nmea_sentences` entries beyond RMC and GGA, which
 * are always on (the fix is built from them). The others reach the UART
 * only — the parser ignores them, so they are never logged. */
#define IDL0_GPS_NMEA_GLL  (1u << 0)
#define IDL0_GPS_NMEA_GSA  (1u << 1)
#define IDL0_GPS_NMEA_GSV  (1u << 2)
#define IDL0_GPS_NMEA_VTG  (1u << 3)
#define IDL0_GPS_NMEA_ZDA  (1u << 4)

/* The §8 `gps` block as applied to the SAM-M10Q (spec §8 "SAM-M10Q
 * (IDL1)"). Loaded by idl0_config.c with every absent field at the
 * module's factory value; also written per session as GPS_CONFIG. */
typedef struct {
    uint8_t  rate_hz;                 /* 1..18 Hz navigation rate */
    uint8_t  signals;                 /* IDL0_GPS_SIG_* bits */
    uint8_t  dynamic_model;           /* UBX_DYNMODEL_* (ubx.h) */
    int8_t   min_elevation_deg;       /* 0..60 deg */
    uint8_t  min_cno_dbhz;            /* 0..50 dBHz */
    uint8_t  min_satellites;          /* 1..32, <= max_satellites */
    uint8_t  max_satellites;          /* 1..32 */
    uint8_t  fix_gate_satellites;     /* 0..32; 0 = off */
    uint8_t  fix_gate_cno_dbhz;       /* 0..50 dBHz */
    uint16_t pdop_mask_x10;           /* 10..250 (PDOP 1.0..25.0) */
    uint16_t position_accuracy_mask_m;/* 1..100 m */
    bool     high_precision_nmea;
    uint8_t  nmea_extra;              /* IDL0_GPS_NMEA_* bits */
} idl0_gps_settings_t;

/* Outcome of the boot-time UBX configuration. Anything but OK leaves the
 * module on its previous settings, still streaming NMEA. */
typedef enum {
    IDL0_GPS_CFG_NOT_RUN = 0,  /* idl0_gps_init not reached the module yet */
    IDL0_GPS_CFG_OK,           /* every VALSET acknowledged */
    IDL0_GPS_CFG_NO_RESPONSE,  /* no MON-VER reply at 9600 or 115200 bit/s */
    IDL0_GPS_CFG_BAUD_FAILED,  /* lost the module while changing rate */
    IDL0_GPS_CFG_NAK,          /* module rejected a VALSET (UBX-ACK-NAK) */
    IDL0_GPS_CFG_NO_ACK,       /* no UBX-ACK within 1.1 s */
    IDL0_GPS_CFG_ENCODE,       /* VALSET did not encode (firmware bug) */
} idl0_gps_cfg_result_t;

idl0_gps_cfg_result_t idl0_gps_config_result(void);
const char *idl0_gps_cfg_result_str(idl0_gps_cfg_result_t r);

/* Navigation rate the module acknowledged, Hz (1, the factory rate, until
 * a rate VALSET is acked). */
uint8_t idl0_gps_applied_rate_hz(void);

/* The settings idl0_gps_init was given (zeroed before it runs). */
const idl0_gps_settings_t *idl0_gps_settings(void);

/* UBX-MON-VER swVersion from the boot probe; "" when the module did not
 * answer. */
const char *idl0_gps_module_version(void);

/* Parsed GPS fix — fields mirror §5.6 GPS_FIX record. */
typedef struct {
    int64_t gps_epoch_ms;
    int64_t device_timestamp_us;
    int32_t latitude_e7;
    int32_t longitude_e7;
    int16_t altitude_x10;
    uint16_t speed_x100;
    uint16_t heading_x100;
    uint8_t fix_quality;
    uint8_t satellites;
    uint16_t hdop_x100;   /* GGA HDOP x 100 (F12); 0 when unknown */
} idl0_gps_fix_t;

/* GPS subsystem state, mapped 1:1 onto the §7.3 GPS: status token. */
typedef enum {
    IDL0_GPS_ABSENT = 0,  /* gps_init not called or failed */
    IDL0_GPS_NOFIX,       /* UART up, no valid RMC received yet */
    IDL0_GPS_FIX,         /* at least one RMC with status='A' seen */
} idl0_gps_state_t;

/* Opens the UART and configures the module with `settings` (blocks up to
 * ~5 s when the module does not answer). Returns false only when the UART
 * itself fails; a configuration failure is reported by
 * idl0_gps_config_result. */
bool idl0_gps_init(const idl0_gps_settings_t *settings);

idl0_gps_state_t idl0_gps_state(void);
const char *idl0_gps_state_str(idl0_gps_state_t state);

/* Latest GGA fix quality (raw NMEA field 6) and satellites used (field 7)
 * for the §7.3 GPSFix/GPSSats lines. Both read 0 while no fresh GGA has
 * arrived (searching, or older than the 3 s staleness window). Returns
 * false when the GPS is ABSENT, so the lines are left out. */
bool idl0_gps_live(uint8_t *fix_quality, uint8_t *satellites);

/* Internal hook used by gps_task's lost-lock watchdog. Forces the
 * driver's state-machine to the given state. Also clears the cached GGA
 * staleness state (T25): a fix reacquired after a force-to-NOFIX must not
 * carry forward altitude/fix_quality/satellites from before the lock was
 * lost — the next RMC emit will re-zero those fields until a fresh GGA
 * arrives. The integrator's pending fix struct itself is left untouched
 * (its lat/lon/time fields are overwritten wholesale by the next RMC
 * anyway). Do not call from anywhere else — normal transitions happen
 * inside idl0_gps_feed_line. */
void idl0_gps_force_state(idl0_gps_state_t state);

/* --- NMEA parser — exposed for host-test --- */

/* Longest NMEA line handled, bytes including the NUL. NMEA's limit is 82
 * characters, but u-blox high-precision mode (§8 gps.high_precision_nmea)
 * adds digits to latitude, longitude and altitude and can pass it. */
#define IDL0_NMEA_LINE_MAX 128

/* Verifies a "$...*HH" NMEA line's XOR checksum. Returns true on match.
 * The trailing CR/LF (if any) must already be stripped. */
bool idl0_nmea_verify_checksum(const char *line);

/* Result of parsing an RMC sentence. Distinguishes a well-formed sentence
 * that reports "no fix" (status='V') from one that could not be parsed at
 * all, so callers can react to "receiver says no fix" (T24) without
 * conflating it with garbage/corrupt input. */
typedef enum {
    IDL0_NMEA_RMC_INVALID = 0,  /* bad checksum, malformed fields, or an
                                 * out-of-range/non-digit date or time
                                 * field (T27/T45) — `fix` untouched */
    IDL0_NMEA_RMC_VOID,         /* well-formed, checksum-valid, status='V'
                                 * (receiver reports no fix) — `fix` untouched */
    IDL0_NMEA_RMC_VALID,        /* well-formed, checksum-valid, status='A' —
                                 * `fix` populated */
} idl0_nmea_rmc_result_t;

/* Parses an RMC sentence. Fields filled on IDL0_NMEA_RMC_VALID:
 * gps_epoch_ms, latitude_e7, longitude_e7, speed_x100, heading_x100.
 * Other fields (and `fix` as a whole on IDL0_NMEA_RMC_INVALID/VOID) are
 * left untouched. See idl0_nmea_rmc_result_t for the three outcomes. */
idl0_nmea_rmc_result_t idl0_nmea_parse_rmc(const char *line, idl0_gps_fix_t *fix);

/* Parses a GGA sentence. Fields filled: altitude_x10, fix_quality,
 * satellites. lat/lon are present in GGA but RMC is canonical — we
 * ignore them here. Returns true on a well-formed, checksum-valid GGA. */
bool idl0_nmea_parse_gga(const char *line, idl0_gps_fix_t *fix);

/* Feed one received NMEA line (no trailing CR/LF) to the integrator.
 * If the line completes a fix (RMC arrives with the latest GGA already
 * folded in), out_fix is populated and the function returns true. The
 * device_timestamp_us field is set from esp_timer_get_time() at the
 * moment the RMC is processed. */
bool idl0_gps_feed_line(const char *line, idl0_gps_fix_t *out_fix);
