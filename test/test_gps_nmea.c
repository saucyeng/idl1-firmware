/* Host-buildable unit test for the gps_driver NMEA parser.
 *
 * Currently inspection-verified only: gps_driver.c mixes the pure-C99
 * parser with UART/log calls, so this file does not link on a host as-is.
 * The parser will be extracted to gps_nmea.c (a follow-up task) at which
 * point the build line becomes:
 *
 *   cc -std=c99 -I../main test_gps_nmea.c ../main/gps_nmea.c -o test_gps_nmea
 *   ./test_gps_nmea
 *
 * Until then the test assertions document expected byte-level behaviour
 * for spec review.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "gps_driver.h"

static int g_checks = 0;
#define CHECK(cond) do { g_checks++; if (!(cond)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); return 1; } } while (0)

static int test_checksum(void) {
    /* Real RMC from a u-blox module — known-good checksum 6A. */
    const char *ok = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A";
    CHECK(idl0_nmea_verify_checksum(ok));
    /* Wrong checksum — flip the last hex char. */
    const char *bad = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6B";
    CHECK(!idl0_nmea_verify_checksum(bad));
    return 0;
}

static int test_rmc(void) {
    idl0_gps_fix_t f = {0};
    /* 12:35:19 UTC on 23 Mar 1994, 48°07.038'N, 11°31.000'E, 22.4 knots, course 084.4. */
    const char *rmc = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A";
    CHECK(idl0_nmea_parse_rmc(rmc, &f) == IDL0_NMEA_RMC_VALID);
    /* Epoch ms for 1994-03-23T12:35:19Z = 764426119000. */
    CHECK(f.gps_epoch_ms == 764426119000LL);
    /* 48 + 07.038/60 = 48.1173 → 481173000 e7 (rounded). */
    CHECK(f.latitude_e7  == 481173000);
    /* 11 + 31.000/60 = 11.51666667 → 115166667. */
    CHECK(f.longitude_e7 == 115166667);
    /* 22.4 knots × 1.852 = 41.4848 km/h → 4148. */
    CHECK(f.speed_x100   == 4148);
    /* 084.4 → 8440. */
    CHECK(f.heading_x100 == 8440);

    /* T24: a well-formed, checksum-valid RMC with status='V' (void fix) is
     * distinct from a parse failure — the receiver is reporting "no fix",
     * not sending garbage. It must report IDL0_NMEA_RMC_VOID, not
     * IDL0_NMEA_RMC_INVALID, and must leave `fix` untouched. */
    const char *void_rmc = "$GPRMC,123519,V,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*7D";
    idl0_gps_fix_t f2 = {0};
    CHECK(idl0_nmea_parse_rmc(void_rmc, &f2) == IDL0_NMEA_RMC_VOID);
    return 0;
}

static int test_invalid_date(void) {
    /* T27: an XOR checksum is order-invariant, so transposing the dd/mm
     * digit pairs within the date field ("230394" -> "032394") leaves the
     * checksum unchanged (still *6A) while producing mo=23 — must be
     * rejected by range validation rather than yielding a wildly wrong
     * gps_epoch_ms with a "successful" parse. */
    idl0_gps_fix_t f = {0};
    const char *transposed_date =
        "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,032394,003.1,W*6A";
    CHECK(idl0_nmea_parse_rmc(transposed_date, &f) == IDL0_NMEA_RMC_INVALID);
    return 0;
}

static int test_glued_sentences(void) {
    /* T26: a corrupted/dropped CR/LF glues two sentences into one UART
     * read with no separator between them — a GGA immediately followed by
     * an RMC, no '\r'/'\n' in between. This documents the case at the
     * parser level: gps_task.c's line assembler (not host-testable here —
     * it depends on uart_read_bytes) now flushes the buffer the instant a
     * mid-line '$' arrives, so each sentence below reaches
     * idl0_gps_feed_line as its own complete line. Splitting at the
     * embedded '$' (exactly what the assembler now does) and parsing each
     * half independently shows both sentences are still recoverable. */
    const char *glued =
        "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47"
        "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A";

    const char *second = strchr(glued + 1, '$');
    CHECK(second != NULL);

    char gga_only[80];
    size_t gga_len = (size_t)(second - glued);
    CHECK(gga_len < sizeof(gga_only));
    memcpy(gga_only, glued, gga_len);
    gga_only[gga_len] = '\0';

    idl0_gps_fix_t fg = {0};
    CHECK(idl0_nmea_parse_gga(gga_only, &fg));
    CHECK(fg.fix_quality == 1);
    CHECK(fg.satellites  == 8);
    CHECK(fg.altitude_x10 == 5454);

    idl0_gps_fix_t fr = {0};
    CHECK(idl0_nmea_parse_rmc(second, &fr) == IDL0_NMEA_RMC_VALID);
    CHECK(fr.latitude_e7 == 481173000);
    return 0;
}

static int test_gga(void) {
    idl0_gps_fix_t f = {0};
    const char *gga = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47";
    CHECK(idl0_nmea_parse_gga(gga, &f));
    CHECK(f.fix_quality == 1);
    CHECK(f.satellites  == 8);
    /* 545.4 m × 10 = 5454. */
    CHECK(f.altitude_x10 == 5454);
    return 0;
}

static int test_southern_western(void) {
    /* Southern/Western hemisphere sign handling. */
    idl0_gps_fix_t f = {0};
    const char *rmc = "$GPRMC,123519,A,3351.000,S,15112.000,E,000.0,000.0,230394,000.0,E*6F";
    CHECK(idl0_nmea_parse_rmc(rmc, &f) == IDL0_NMEA_RMC_VALID);
    /* -(33 + 51/60) = -33.85 → -338500000. */
    CHECK(f.latitude_e7 == -338500000);
    /* +(151 + 12/60) = 151.2 → 1512000000. */
    CHECK(f.longitude_e7 == 1512000000);
    return 0;
}

int main(void) {
    if (test_checksum())          return 1;
    if (test_rmc())               return 1;
    if (test_gga())               return 1;
    if (test_southern_western())  return 1;
    if (test_invalid_date())      return 1;
    if (test_glued_sentences())   return 1;
    printf("PASS — %d checks\n", g_checks);
    return 0;
}
