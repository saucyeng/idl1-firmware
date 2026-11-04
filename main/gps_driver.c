/* GPS driver — see gps_driver.h.
 *
 * P6 milestone 2: state stub only. The NMEA parser (M3) and UART task
 * (M4) populate the rest. */

#include "gps_driver.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_timer.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "pins.h"
#include "ubx.h"

static idl0_gps_state_t s_state = IDL0_GPS_ABSENT;

/* Integrator state — RMC drives emit; latest GGA is folded in. Declared
 * here (rather than beside idl0_gps_feed_line, where they're used) so
 * idl0_gps_force_state below can clear the GGA staleness tracking (T25). */
static idl0_gps_fix_t s_pending     = {0};
static bool           s_gga_seen    = false;
static int64_t        s_last_gga_us = 0;  /* esp_timer_get_time() at last successful GGA parse, us */

idl0_gps_state_t idl0_gps_state(void) { return s_state; }

void idl0_gps_force_state(idl0_gps_state_t state)
{
    s_state = state;
    /* T25: a fix reacquired after this force (e.g. lost-lock -> NOFIX)
     * must not carry forward GGA data cached before the lock was lost. */
    s_gga_seen    = false;
    s_last_gga_us = 0;
}

const char *idl0_gps_state_str(idl0_gps_state_t state)
{
    switch (state) {
        case IDL0_GPS_FIX:    return "FIX";
        case IDL0_GPS_NOFIX:  return "NOFIX";
        case IDL0_GPS_ABSENT:
        default:              return "ABSENT";
    }
}

/* --- Helpers (pure C; no ESP-IDF references) ---------------------- */

/* Days from 1970-01-01 for the given proleptic Gregorian date.
 * Howard Hinnant's days_from_civil — handles all valid GPS dates. */
static int64_t days_from_civil(int y, unsigned m, unsigned d)
{
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153u * (m > 2 ? m - 3 : m + 9) + 2u) / 5u + d - 1u;
    const unsigned doe = yoe * 365u + yoe/4u - yoe/100u + doy;
    return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

/* Convert "DDMM.mmmm" (lat) or "DDDMM.mmmm" (lon) + hemisphere char
 * into degrees × 1e7. Returns 0 on a blank/invalid field. */
static int32_t parse_lat_lon(const char *dmm, char hemi)
{
    if (dmm == NULL || *dmm == '\0') return 0;
    double v = atof(dmm);
    double deg_whole = (double)((int)v / 100);          /* truncate to whole degrees */
    double minutes   = v - deg_whole * 100.0;
    double deg       = deg_whole + minutes / 60.0;
    if (hemi == 'S' || hemi == 'W') deg = -deg;
    /* Round-to-nearest, away from zero. */
    double scaled = deg * 1e7;
    return (int32_t)(scaled >= 0 ? scaled + 0.5 : scaled - 0.5);
}

/* Token splitter — replaces ',' with '\0' in `buf` and writes pointers
 * into `out` for up to `max` tokens. Returns the token count. The '*'
 * before the checksum is also treated as a terminator. */
static size_t split_csv(char *buf, char *out[], size_t max)
{
    size_t n = 0;
    out[n++] = buf;
    for (char *p = buf; *p != '\0' && n < max; p++) {
        if (*p == ',' || *p == '*') {
            *p = '\0';
            out[n++] = p + 1;
        }
    }
    return n;
}

bool idl0_nmea_verify_checksum(const char *line)
{
    if (line == NULL || line[0] != '$') return false;
    uint8_t sum = 0;
    const char *p = line + 1;
    while (*p != '\0' && *p != '*') {
        sum ^= (uint8_t)*p;
        p++;
    }
    if (*p != '*' || !isxdigit((unsigned char)p[1]) || !isxdigit((unsigned char)p[2])) {
        return false;
    }
    char hex[3] = { p[1], p[2], '\0' };
    return (uint8_t)strtol(hex, NULL, 16) == sum;
}

idl0_nmea_rmc_result_t idl0_nmea_parse_rmc(const char *line, idl0_gps_fix_t *fix)
{
    if (!idl0_nmea_verify_checksum(line)) return IDL0_NMEA_RMC_INVALID;
    /* Talker prefix may be GP, GN, GL, GA — match on the message id only. */
    if (strlen(line) < 6 || strncmp(line + 3, "RMC", 3) != 0) return IDL0_NMEA_RMC_INVALID;

    char buf[IDL0_NMEA_LINE_MAX];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *t[16] = {0};
    size_t n = split_csv(buf, t, 16);
    if (n < 10) return IDL0_NMEA_RMC_INVALID;

    /* RMC field order:
     *  [0]=$xxRMC  [1]=hhmmss.ss  [2]=status (A/V)
     *  [3]=lat     [4]=N/S        [5]=lon       [6]=E/W
     *  [7]=speed (knots)  [8]=course (deg true) [9]=ddmmyy */
    if (t[2][0] != 'A' && t[2][0] != 'V') return IDL0_NMEA_RMC_INVALID;  /* malformed status field */
    /* T24: status='V' is a well-formed sentence saying "no fix" — distinct
     * from a parse failure. Report it as such; don't touch `fix`. */
    if (t[2][0] == 'V') return IDL0_NMEA_RMC_VOID;

    /* Time hhmmss.ss → hh, mm, ss, ms. Every consumed digit is range-checked
     * (T27) — an XOR checksum is order-invariant, so a transposed field
     * still passes it and must be caught here instead. */
    const char *tm = t[1];
    size_t tm_len = strlen(tm);
    if (tm_len < 6) return IDL0_NMEA_RMC_INVALID;
    for (int i = 0; i < 6; i++) {
        if (!isdigit((unsigned char)tm[i])) return IDL0_NMEA_RMC_INVALID;
    }
    int hh = (tm[0]-'0')*10 + (tm[1]-'0');
    int mm = (tm[2]-'0')*10 + (tm[3]-'0');
    int ss = (tm[4]-'0')*10 + (tm[5]-'0');
    if (hh > 23 || mm > 59 || ss > 60) return IDL0_NMEA_RMC_INVALID;  /* ss==60: leap second */

    int ms = 0;
    if (tm[6] == '.') {
        /* T45: stop at the field's actual length — don't read past it on
         * the assumption the next comma-terminated field starts right
         * after 3 digits. */
        for (int i = 0; i < 3; i++) {
            size_t idx = 7 + (size_t)i;
            if (idx >= tm_len || !isdigit((unsigned char)tm[idx])) break;
            ms = ms * 10 + (tm[idx] - '0');
        }
    }

    /* Date ddmmyy → d, m, y (2000-relative). Digit/range-checked (T27). */
    const char *dt = t[9];
    if (strlen(dt) < 6) return IDL0_NMEA_RMC_INVALID;
    for (int i = 0; i < 6; i++) {
        if (!isdigit((unsigned char)dt[i])) return IDL0_NMEA_RMC_INVALID;
    }
    int dd = (dt[0]-'0')*10 + (dt[1]-'0');
    int mo = (dt[2]-'0')*10 + (dt[3]-'0');
    int yy = (dt[4]-'0')*10 + (dt[5]-'0');
    if (dd < 1 || dd > 31 || mo < 1 || mo > 12) return IDL0_NMEA_RMC_INVALID;
    int year = 2000 + yy;

    int64_t days = days_from_civil(year, (unsigned)mo, (unsigned)dd);
    int64_t secs = days * 86400 + (int64_t)hh * 3600 + (int64_t)mm * 60 + ss;
    fix->gps_epoch_ms = secs * 1000 + ms;

    fix->latitude_e7  = parse_lat_lon(t[3], t[4][0]);
    fix->longitude_e7 = parse_lat_lon(t[5], t[6][0]);

    double knots = (t[7][0] != '\0') ? atof(t[7]) : 0.0;
    double kmh   = knots * 1.852;
    double kmh_x100 = kmh * 100.0 + 0.5;
    fix->speed_x100 = (kmh_x100 < 0.0)        ? 0
                    : (kmh_x100 > 65535.0)    ? 65535
                    : (uint16_t)kmh_x100;

    double course = (t[8][0] != '\0') ? atof(t[8]) : 0.0;
    double course_x100 = course * 100.0 + 0.5;
    fix->heading_x100 = (course_x100 < 0.0)     ? 0
                      : (course_x100 > 65535.0) ? 65535
                      : (uint16_t)course_x100;

    return IDL0_NMEA_RMC_VALID;
}

bool idl0_nmea_parse_gga(const char *line, idl0_gps_fix_t *fix)
{
    if (!idl0_nmea_verify_checksum(line)) return false;
    if (strlen(line) < 6 || strncmp(line + 3, "GGA", 3) != 0) return false;

    char buf[IDL0_NMEA_LINE_MAX];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *t[16] = {0};
    size_t n = split_csv(buf, t, 16);
    if (n < 10) return false;

    /* GGA fields:
     *  [0]=$xxGGA  [1]=hhmmss.ss  [2]=lat [3]=N/S
     *  [4]=lon     [5]=E/W        [6]=fix_quality (0..9)
     *  [7]=satellites [8]=hdop    [9]=altitude (m) [10]=altitude unit */

    int fq = atoi(t[6]);
    fix->fix_quality = (fq < 0) ? 0 : (fq > 255 ? 255 : (uint8_t)fq);

    int sats = atoi(t[7]);
    fix->satellites = (sats < 0) ? 0 : (sats > 255 ? 255 : (uint8_t)sats);

    /* HDOP (F12), x 100 to fit a u16: "0.85" -> 85. */
    double hdop = (t[8][0] != '\0') ? atof(t[8]) : 0.0;
    double hdop_x100 = hdop * 100.0 + 0.5;
    fix->hdop_x100 = (hdop_x100 < 0.0) ? 0 : (hdop_x100 > 65535.0) ? 65535 : (uint16_t)hdop_x100;

    double alt = (t[9][0] != '\0') ? atof(t[9]) : 0.0;
    double alt_x10 = alt * 10.0 + (alt >= 0.0 ? 0.5 : -0.5);
    fix->altitude_x10 = (alt_x10 < -32768.0) ? -32768
                      : (alt_x10 >  32767.0) ?  32767
                      : (int16_t)alt_x10;

    return true;
}

/* Integrator — RMC drives emit; latest GGA is folded in. State variables
 * (s_pending, s_gga_seen, s_last_gga_us) live near s_state at the top of
 * this file so idl0_gps_force_state can reach them. */

/* T25: a GGA older than this is too stale to fold into an RMC emit —
 * zero altitude/fix_quality/satellites instead of repeating pre-loss
 * data. 3,000,000 us = 3 s (three missed 1 Hz GGA sentences). */
#define IDL0_GPS_GGA_STALE_US (3 * 1000 * 1000LL)

bool idl0_gps_feed_line(const char *line, idl0_gps_fix_t *out_fix)
{
    if (line == NULL || line[0] != '$') return false;

    /* GGA: update the rolling altitude/quality/sats. */
    if (strlen(line) >= 6 && strncmp(line + 3, "GGA", 3) == 0) {
        if (idl0_nmea_parse_gga(line, &s_pending)) {
            s_gga_seen    = true;
            s_last_gga_us = esp_timer_get_time();
        }
        return false;
    }

    /* RMC: emit on status='A'; carry forward latest GGA if it's fresh. */
    if (strlen(line) >= 6 && strncmp(line + 3, "RMC", 3) == 0) {
        idl0_nmea_rmc_result_t r = idl0_nmea_parse_rmc(line, &s_pending);
        if (r == IDL0_NMEA_RMC_INVALID) return false;
        if (r == IDL0_NMEA_RMC_VOID) {
            /* T24: a well-formed "no fix" report — drop to NOFIX now
             * instead of waiting on the 5 s silence watchdog. No fix to
             * emit. */
            s_state = IDL0_GPS_NOFIX;
            return false;
        }
        s_pending.device_timestamp_us = esp_timer_get_time();
        bool gga_stale = !s_gga_seen ||
            (s_pending.device_timestamp_us - s_last_gga_us) > IDL0_GPS_GGA_STALE_US;
        if (gga_stale) {
            /* GGA hasn't arrived yet, or is too old (T25) — emit a fix
             * with altitude/quality/sats=0 rather than stale/absent data. */
            s_pending.altitude_x10 = 0;
            s_pending.fix_quality  = 0;
            s_pending.satellites   = 0;
            s_pending.hdop_x100    = 0;
        }
        if (s_state == IDL0_GPS_NOFIX) s_state = IDL0_GPS_FIX;
        *out_fix = s_pending;
        return true;
    }

    return false;  /* unhandled sentence — silently ignored */
}

bool idl0_gps_live(uint8_t *fix_quality, uint8_t *satellites)
{
    if (s_state == IDL0_GPS_ABSENT) return false;
    /* Single-byte fields written by the GPS task; a torn read is not
     * possible, and a one-second-stale value is fine for a status line. */
    const bool fresh = s_gga_seen &&
        (esp_timer_get_time() - s_last_gga_us) <= IDL0_GPS_GGA_STALE_US;
    *fix_quality = fresh ? s_pending.fix_quality : 0;
    *satellites  = fresh ? s_pending.satellites  : 0;
    return true;
}

#define IDL0_GPS_UART       UART_NUM_1
#define IDL0_GPS_RX_BUF     2048

/* SAM-M10Q factory UART: 9600 bit/s 8N1 (data sheet UBX-22013293 Table 18).
 * The firmware moves it to 115200 bit/s: RMC + GGA at 10 Hz is ~1.5 kB/s,
 * over the ~0.96 kB/s 9600 bit/s carries, and the optional sentences add
 * more. The module keeps the new rate (RAM layer) until it loses power,
 * so an ESP reset can find it at either rate — init probes both. */
#define IDL0_GPS_BAUD_FACTORY  9600    /* bit/s */
#define IDL0_GPS_BAUD_RUN      115200  /* bit/s */

/* UBX-ACK arrives "as soon as possible but at least within one second"
 * (Interface Description UBX-21035062 §3.9.1); a MON-VER poll answers as
 * fast. 1100 ms leaves a margin over that bound. */
#define IDL0_GPS_UBX_TIMEOUT_MS  1100  /* ms */

/* A CFG-SIGNAL change restarts the GNSS subsystem: "wait first for the
 * acknowledgement from the receiver and then 0.5 seconds before sending
 * the next command" (ID §4.9.21). */
#define IDL0_GPS_SIGNAL_SETTLE_MS  500  /* ms */

/* The module powers up on the PWR_EN rail early in app_main, a few hundred
 * ms before init runs, and ignores UBX for a while after: on hardware, MON-VER
 * polls in the first ~2 s after power-on went unanswered every boot (though
 * it was already sending NMEA), and one at ~4 s was answered. Init keeps
 * polling at the factory rate until this long has passed. */
#define IDL0_GPS_STARTUP_WAIT_MS  5000  /* ms, from init */


static const char *TAG = "gps";

static idl0_gps_cfg_result_t s_cfg_result = IDL0_GPS_CFG_NOT_RUN;
static uint8_t s_applied_rate_hz = 1;  /* Hz; factory default until a VALSET is acked */
static idl0_gps_settings_t s_settings;  /* what idl0_gps_init was given */
static char s_module_sw[31];            /* MON-VER swVersion, CH[30] + NUL */

idl0_gps_cfg_result_t idl0_gps_config_result(void) { return s_cfg_result; }
uint8_t idl0_gps_applied_rate_hz(void) { return s_applied_rate_hz; }
const idl0_gps_settings_t *idl0_gps_settings(void) { return &s_settings; }
const char *idl0_gps_module_version(void) { return s_module_sw; }

const char *idl0_gps_cfg_result_str(idl0_gps_cfg_result_t r)
{
    switch (r) {
        case IDL0_GPS_CFG_OK:          return "OK";
        case IDL0_GPS_CFG_NO_RESPONSE: return "NO_RESPONSE";
        case IDL0_GPS_CFG_BAUD_FAILED: return "BAUD_FAILED";
        case IDL0_GPS_CFG_NAK:         return "NAK";
        case IDL0_GPS_CFG_NO_ACK:      return "NO_ACK";
        case IDL0_GPS_CFG_ENCODE:      return "ENCODE";
        case IDL0_GPS_CFG_NOT_RUN:
        default:                       return "NOT_RUN";
    }
}

/* Reads the UART into `p` until a frame matching cls/id completes (true)
 * or the timeout passes (false). Bytes of other traffic, NMEA included,
 * are consumed and dropped — only called before the GPS task starts. */
static bool ubx_wait_frame(ubx_parser_t *p, uint8_t cls, uint8_t id, int timeout_ms)
{
    const int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    uint8_t chunk[64];
    while (esp_timer_get_time() < deadline) {
        int n = uart_read_bytes(IDL0_GPS_UART, chunk, sizeof(chunk), pdMS_TO_TICKS(20));
        for (int i = 0; i < n; i++) {
            if (ubx_parser_feed(p, chunk[i]) && p->cls == cls && p->id == id) {
                return true;
            }
        }
    }
    return false;
}

/* Polls UBX-MON-VER at the UART's current rate. True when the module
 * answers; its swVersion string is logged. */
static bool ubx_probe(void)
{
    uint8_t frame[UBX_FRAME_OVERHEAD];
    size_t n = ubx_frame(frame, sizeof(frame), UBX_CLASS_MON, UBX_ID_MON_VER, NULL, 0);
    uart_flush_input(IDL0_GPS_UART);
    uart_write_bytes(IDL0_GPS_UART, frame, n);

    ubx_parser_t p;
    ubx_parser_init(&p);
    if (!ubx_wait_frame(&p, UBX_CLASS_MON, UBX_ID_MON_VER, IDL0_GPS_UBX_TIMEOUT_MS) ||
        p.len < 40) {
        return false;
    }
    char hw[11];
    memcpy(s_module_sw, p.payload, 30); s_module_sw[30] = '\0';  /* CH[30] swVersion */
    memcpy(hw, p.payload + 30, 10);     hw[10] = '\0';           /* CH[10] hwVersion */
    ESP_LOGI(TAG, "module: sw \"%s\" hw \"%s\"", s_module_sw, hw);
    return true;
}

/* Sends a VALSET and waits for its ACK. */
static idl0_gps_cfg_result_t ubx_valset_send(const ubx_valset_t *v)
{
    uint8_t frame[sizeof(v->buf) + UBX_FRAME_OVERHEAD];
    size_t n = ubx_valset_frame(v, frame, sizeof(frame));
    if (n == 0) return IDL0_GPS_CFG_ENCODE;
    uart_write_bytes(IDL0_GPS_UART, frame, n);

    ubx_parser_t p;
    ubx_parser_init(&p);
    const int64_t deadline = esp_timer_get_time() + (int64_t)IDL0_GPS_UBX_TIMEOUT_MS * 1000;
    uint8_t chunk[64];
    while (esp_timer_get_time() < deadline) {
        int got = uart_read_bytes(IDL0_GPS_UART, chunk, sizeof(chunk), pdMS_TO_TICKS(20));
        for (int i = 0; i < got; i++) {
            if (!ubx_parser_feed(&p, chunk[i])) continue;
            switch (ubx_parser_ack_for(&p, UBX_CLASS_CFG, UBX_ID_CFG_VALSET)) {
                case UBX_ACK_ACK: return IDL0_GPS_CFG_OK;
                case UBX_ACK_NAK: return IDL0_GPS_CFG_NAK;
                case UBX_ACK_NONE: break;
            }
        }
    }
    return IDL0_GPS_CFG_NO_ACK;
}

/* Moves module and host from the factory rate to IDL0_GPS_BAUD_RUN. The
 * ACK for a baud change may be sent at either rate, so it is not waited
 * on: the MON-VER probe at the new rate is the proof. */
static bool ubx_switch_baud(void)
{
    ubx_valset_t v;
    ubx_valset_init(&v, UBX_LAYER_RAM);
    ubx_valset_add(&v, UBX_CFG_UART1_BAUDRATE, IDL0_GPS_BAUD_RUN);
    uint8_t frame[32];
    size_t n = ubx_valset_frame(&v, frame, sizeof(frame));
    if (n == 0) return false;
    uart_write_bytes(IDL0_GPS_UART, frame, n);
    uart_wait_tx_done(IDL0_GPS_UART, pdMS_TO_TICKS(100));
    vTaskDelay(pdMS_TO_TICKS(100));  /* let the module finish and switch */
    uart_set_baudrate(IDL0_GPS_UART, IDL0_GPS_BAUD_RUN);
    return ubx_probe();
}

/* Applies the §8 `gps` settings (RAM layer). Runs once at boot with the UART
 * to itself, before the GPS task reads it. A failure leaves the module on
 * whatever it was running — still NMEA at ≥1 Hz — and is reported. */
/* Polls MON-VER at the factory rate until the module answers or
 * IDL0_GPS_STARTUP_WAIT_MS has passed. */
static bool ubx_probe_startup(void)
{
    const int64_t deadline = esp_timer_get_time() + (int64_t)IDL0_GPS_STARTUP_WAIT_MS * 1000;
    do {
        if (ubx_probe()) return true;
    } while (esp_timer_get_time() < deadline);
    return false;
}

static idl0_gps_cfg_result_t gps_configure(const idl0_gps_settings_t *cfg)
{
    /* Find the module: factory rate first (cold power-up, waiting out its
     * start), then the run rate (an ESP reset with the module still
     * powered). */
    bool at_run_baud = false;
    if (!ubx_probe_startup()) {
        uart_set_baudrate(IDL0_GPS_UART, IDL0_GPS_BAUD_RUN);
        if (!ubx_probe()) {
            uart_set_baudrate(IDL0_GPS_UART, IDL0_GPS_BAUD_FACTORY);
            return IDL0_GPS_CFG_NO_RESPONSE;
        }
        at_run_baud = true;
    }
    if (!at_run_baud && !ubx_switch_baud()) {
        /* Lost it mid-switch: try the factory rate again, else give up. */
        uart_set_baudrate(IDL0_GPS_UART, IDL0_GPS_BAUD_FACTORY);
        if (!ubx_probe()) return IDL0_GPS_CFG_BAUD_FAILED;
        ESP_LOGW(TAG, "baud switch to %d failed — staying at %d",
                 IDL0_GPS_BAUD_RUN, IDL0_GPS_BAUD_FACTORY);
    }

    const uint8_t rate_hz = (cfg->rate_hz < 1) ? 1 : (cfg->rate_hz > 18 ? 18 : cfg->rate_hz);
    ubx_valset_t v;
    ubx_valset_init(&v, UBX_LAYER_RAM);
    /* round(1000 / rate) ms: 3 Hz -> 333 ms, 18 Hz -> 56 ms (min 25). */
    ubx_valset_add(&v, UBX_CFG_RATE_MEAS, (uint64_t)((1000 + rate_hz / 2) / rate_hz));
    ubx_valset_add(&v, UBX_CFG_RATE_NAV, 1);  /* one solution per measurement */
    ubx_valset_add(&v, UBX_CFG_MSGOUT_NMEA_RMC_UART1, 1);
    ubx_valset_add(&v, UBX_CFG_MSGOUT_NMEA_GGA_UART1, 1);
    ubx_valset_add(&v, UBX_CFG_MSGOUT_NMEA_GLL_UART1, (cfg->nmea_extra & IDL0_GPS_NMEA_GLL) ? 1 : 0);
    ubx_valset_add(&v, UBX_CFG_MSGOUT_NMEA_GSA_UART1, (cfg->nmea_extra & IDL0_GPS_NMEA_GSA) ? 1 : 0);
    ubx_valset_add(&v, UBX_CFG_MSGOUT_NMEA_GSV_UART1, (cfg->nmea_extra & IDL0_GPS_NMEA_GSV) ? 1 : 0);
    ubx_valset_add(&v, UBX_CFG_MSGOUT_NMEA_VTG_UART1, (cfg->nmea_extra & IDL0_GPS_NMEA_VTG) ? 1 : 0);
    ubx_valset_add(&v, UBX_CFG_MSGOUT_NMEA_ZDA_UART1, (cfg->nmea_extra & IDL0_GPS_NMEA_ZDA) ? 1 : 0);
    ubx_valset_add(&v, UBX_CFG_NMEA_HIGHPREC, cfg->high_precision_nmea ? 1 : 0);
    ubx_valset_add(&v, UBX_CFG_NAVSPG_DYNMODEL, cfg->dynamic_model);
    ubx_valset_add(&v, UBX_CFG_NAVSPG_INFIL_MINELEV, (uint8_t)cfg->min_elevation_deg);  /* I1 */
    ubx_valset_add(&v, UBX_CFG_NAVSPG_INFIL_MINCNO, cfg->min_cno_dbhz);
    ubx_valset_add(&v, UBX_CFG_NAVSPG_INFIL_MINSVS, cfg->min_satellites);
    ubx_valset_add(&v, UBX_CFG_NAVSPG_INFIL_MAXSVS, cfg->max_satellites);
    ubx_valset_add(&v, UBX_CFG_NAVSPG_INFIL_NCNOTHRS, cfg->fix_gate_satellites);
    ubx_valset_add(&v, UBX_CFG_NAVSPG_INFIL_CNOTHRS, cfg->fix_gate_cno_dbhz);
    ubx_valset_add(&v, UBX_CFG_NAVSPG_OUTFIL_PDOP, cfg->pdop_mask_x10);
    ubx_valset_add(&v, UBX_CFG_NAVSPG_OUTFIL_PACC, cfg->position_accuracy_mask_m);
    idl0_gps_cfg_result_t r = ubx_valset_send(&v);
    if (r != IDL0_GPS_CFG_OK) return r;
    s_applied_rate_hz = rate_hz;

    /* Signals in a VALSET of their own: any CFG-SIGNAL change restarts the
     * GNSS subsystem. Sent every boot, since the module may hold a previous
     * boot's set. */
    ubx_valset_init(&v, UBX_LAYER_RAM);
    ubx_valset_add(&v, UBX_CFG_SIGNAL_GPS_ENA,  (cfg->signals & IDL0_GPS_SIG_GPS)     ? 1 : 0);
    ubx_valset_add(&v, UBX_CFG_SIGNAL_GAL_ENA,  (cfg->signals & IDL0_GPS_SIG_GALILEO) ? 1 : 0);
    ubx_valset_add(&v, UBX_CFG_SIGNAL_BDS_ENA,  (cfg->signals & IDL0_GPS_SIG_BEIDOU)  ? 1 : 0);
    ubx_valset_add(&v, UBX_CFG_SIGNAL_GLO_ENA,  (cfg->signals & IDL0_GPS_SIG_GLONASS) ? 1 : 0);
    ubx_valset_add(&v, UBX_CFG_SIGNAL_QZSS_ENA, (cfg->signals & IDL0_GPS_SIG_QZSS)    ? 1 : 0);
    ubx_valset_add(&v, UBX_CFG_SIGNAL_SBAS_ENA, (cfg->signals & IDL0_GPS_SIG_SBAS)    ? 1 : 0);
    r = ubx_valset_send(&v);
    vTaskDelay(pdMS_TO_TICKS(IDL0_GPS_SIGNAL_SETTLE_MS));
    return r;
}

bool idl0_gps_init(const idl0_gps_settings_t *settings)
{
    const uart_config_t cfg = {
        .baud_rate  = IDL0_GPS_BAUD_FACTORY,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t err = uart_driver_install(IDL0_GPS_UART, IDL0_GPS_RX_BUF, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install: %s", esp_err_to_name(err));
        s_state = IDL0_GPS_ABSENT;
        return false;
    }
    err = uart_param_config(IDL0_GPS_UART, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config: %s", esp_err_to_name(err));
        uart_driver_delete(IDL0_GPS_UART);  /* T28: undo the install above */
        s_state = IDL0_GPS_ABSENT;
        return false;
    }
    /* IDL1 wires host TX (GPIO16) to the module's RXD, so the firmware can
     * configure it over UBX (IDL0's TX trace was open). */
    err = uart_set_pin(IDL0_GPS_UART,
                       IDL1_PIN_GPS_TX,      /* tx */
                       IDL1_PIN_GPS_RX,      /* rx */
                       UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin: %s", esp_err_to_name(err));
        uart_driver_delete(IDL0_GPS_UART);  /* T28: undo the install above */
        s_state = IDL0_GPS_ABSENT;
        return false;
    }
    s_state = IDL0_GPS_NOFIX;

    /* A configuration failure is reported, not fatal: the module still
     * streams NMEA at whatever it was last set to. */
    s_settings = *settings;
    s_cfg_result = gps_configure(settings);
    if (s_cfg_result == IDL0_GPS_CFG_OK) {
        ESP_LOGI(TAG, "GPS configured: %u Hz, signals 0x%02x, dynamic model %u, extra NMEA 0x%02x",
                 (unsigned)s_applied_rate_hz, (unsigned)settings->signals,
                 (unsigned)settings->dynamic_model, (unsigned)settings->nmea_extra);
    } else {
        ESP_LOGE(TAG, "GPS configuration failed (%s) — module left at its current settings",
                 idl0_gps_cfg_result_str(s_cfg_result));
    }
    uart_flush_input(IDL0_GPS_UART);
    return true;
}
