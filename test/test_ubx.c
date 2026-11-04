/* Host-compiled unit test for ubx.c (u-blox UBX framing, CFG-VALSET
 * encoding, frame parsing).
 *
 * Build & run (from test/):
 *   cc -std=c99 -I../main test_ubx.c ../main/ubx.c -o test_ubx
 *   ./test_ubx
 *
 * ubx.c has no ESP-IDF dependency, so any host C compiler works.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ubx.h"

static int g_checks = 0;
#define CHECK(cond) do { g_checks++; if (!(cond)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); return 1; } } while (0)

/* Feeds `n` bytes; returns how many complete frames were seen. */
static int feed_all(ubx_parser_t *p, const uint8_t *b, size_t n)
{
    int frames = 0;
    for (size_t i = 0; i < n; i++) {
        if (ubx_parser_feed(p, b[i])) frames++;
    }
    return frames;
}

static int test_checksum(void) {
    /* MON-VER poll, worked by hand from ID §3.4 over 0A 04 00 00:
     * a: 0A 0E 0E 0E, b: 0A 18 26 34. */
    uint8_t out[16];
    size_t n = ubx_frame(out, sizeof(out), UBX_CLASS_MON, UBX_ID_MON_VER, NULL, 0);
    const uint8_t want[] = { 0xB5, 0x62, 0x0A, 0x04, 0x00, 0x00, 0x0E, 0x34 };
    CHECK(n == sizeof(want));
    CHECK(memcmp(out, want, sizeof(want)) == 0);
    CHECK(ubx_frame(out, 7, UBX_CLASS_MON, UBX_ID_MON_VER, NULL, 0) == 0);  /* no room */
    return 0;
}

static int test_valset_encoding(void) {
    ubx_valset_t v;
    ubx_valset_init(&v, UBX_LAYER_RAM);
    CHECK(ubx_valset_add(&v, UBX_CFG_RATE_MEAS, 200));              /* U2 */
    CHECK(ubx_valset_add(&v, UBX_CFG_MSGOUT_NMEA_GSV_UART1, 0));    /* U1 */
    CHECK(ubx_valset_add(&v, UBX_CFG_SIGNAL_SBAS_ENA, 1));          /* L, one byte */
    CHECK(ubx_valset_add(&v, UBX_CFG_UART1_BAUDRATE, 115200));      /* U4 */
    CHECK(v.keys == 4);
    const uint8_t want[] = {
        0x00, 0x01, 0x00, 0x00,                          /* version 0, RAM, reserved */
        0x01, 0x00, 0x21, 0x30, 0xC8, 0x00,              /* RATE-MEAS = 200 ms */
        0xC5, 0x00, 0x91, 0x20, 0x00,                    /* MSGOUT GSV = 0 */
        0x20, 0x00, 0x31, 0x10, 0x01,                    /* SBAS_ENA = 1 */
        0x01, 0x00, 0x52, 0x40, 0x00, 0xC2, 0x01, 0x00,  /* BAUDRATE = 115200 */
    };
    CHECK(v.len == sizeof(want));
    CHECK(memcmp(v.buf, want, sizeof(want)) == 0);

    uint8_t frame[64];
    size_t n = ubx_valset_frame(&v, frame, sizeof(frame));
    CHECK(n == sizeof(want) + UBX_FRAME_OVERHEAD);
    CHECK(frame[2] == UBX_CLASS_CFG && frame[3] == UBX_ID_CFG_VALSET);
    CHECK(frame[4] == sizeof(want) && frame[5] == 0);

    /* The framed VALSET parses back intact. */
    ubx_parser_t p;
    ubx_parser_init(&p);
    CHECK(feed_all(&p, frame, n) == 1);
    CHECK(p.cls == UBX_CLASS_CFG && p.id == UBX_ID_CFG_VALSET);
    CHECK(p.len == sizeof(want) && memcmp(p.payload, want, sizeof(want)) == 0);
    return 0;
}

static int test_valset_limits(void) {
    ubx_valset_t v;
    ubx_valset_init(&v, UBX_LAYER_RAM);
    uint8_t frame[16];
    CHECK(ubx_valset_frame(&v, frame, sizeof(frame)) == 0);   /* no keys */
    CHECK(!ubx_valset_add(&v, 0x00000001u, 0));               /* size bits 0: unknown */
    CHECK(v.overflow);

    ubx_valset_init(&v, UBX_LAYER_RAM);
    for (int i = 0; i < UBX_VALSET_MAX_KEYS; i++) {
        CHECK(ubx_valset_add(&v, UBX_CFG_MSGOUT_NMEA_GSV_UART1, 0));
    }
    CHECK(!ubx_valset_add(&v, UBX_CFG_MSGOUT_NMEA_GSV_UART1, 0));  /* 65th key */
    uint8_t big[1024];
    CHECK(ubx_valset_frame(&v, big, sizeof(big)) == 0);
    return 0;
}

static int test_parser_ack_among_nmea(void) {
    /* An ACK-ACK for CFG-VALSET between NMEA sentences, with a stray 0xB5
     * just before the real sync. */
    uint8_t ack[10];
    const uint8_t body[2] = { UBX_CLASS_CFG, UBX_ID_CFG_VALSET };
    size_t an = ubx_frame(ack, sizeof(ack), UBX_CLASS_ACK, UBX_ID_ACK_ACK, body, 2);
    CHECK(an == 10);

    uint8_t stream[128];
    size_t n = 0;
    const char *nmea = "$GNRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,,*00\r\n";
    memcpy(stream + n, nmea, strlen(nmea)); n += strlen(nmea);
    stream[n++] = 0xB5;
    memcpy(stream + n, ack, an); n += an;
    memcpy(stream + n, "$GNGGA*00\r\n", 11); n += 11;

    ubx_parser_t p;
    ubx_parser_init(&p);
    int frames = 0;
    ubx_ack_t got = UBX_ACK_NONE;
    for (size_t i = 0; i < n; i++) {
        if (ubx_parser_feed(&p, stream[i])) {
            frames++;
            got = ubx_parser_ack_for(&p, UBX_CLASS_CFG, UBX_ID_CFG_VALSET);
        }
    }
    CHECK(frames == 1);
    CHECK(got == UBX_ACK_ACK);
    CHECK(ubx_parser_ack_for(&p, UBX_CLASS_MON, UBX_ID_MON_VER) == UBX_ACK_NONE);

    /* A NAK for the same message reads as NAK. */
    an = ubx_frame(ack, sizeof(ack), UBX_CLASS_ACK, UBX_ID_ACK_NAK, body, 2);
    ubx_parser_init(&p);
    CHECK(feed_all(&p, ack, an) == 1);
    CHECK(ubx_parser_ack_for(&p, UBX_CLASS_CFG, UBX_ID_CFG_VALSET) == UBX_ACK_NAK);
    return 0;
}

static int test_parser_rejects(void) {
    uint8_t ack[10];
    const uint8_t body[2] = { UBX_CLASS_CFG, UBX_ID_CFG_VALSET };
    size_t an = ubx_frame(ack, sizeof(ack), UBX_CLASS_ACK, UBX_ID_ACK_ACK, body, 2);

    ubx_parser_t p;
    ubx_parser_init(&p);
    ack[an - 1] ^= 0xFF;                         /* bad CK_B */
    CHECK(feed_all(&p, ack, an) == 0);
    ack[an - 1] ^= 0xFF;
    CHECK(feed_all(&p, ack, an) == 1);           /* resyncs on the next frame */

    /* A declared length past the buffer is dropped, and the parser recovers. */
    const uint8_t huge[] = { 0xB5, 0x62, 0x0A, 0x04, 0xFF, 0x7F };
    CHECK(feed_all(&p, huge, sizeof(huge)) == 0);
    CHECK(feed_all(&p, ack, an) == 1);

    /* A zero-length frame completes. */
    uint8_t poll[8];
    size_t pn = ubx_frame(poll, sizeof(poll), UBX_CLASS_MON, UBX_ID_MON_VER, NULL, 0);
    CHECK(feed_all(&p, poll, pn) == 1);
    CHECK(p.len == 0);
    return 0;
}

int main(void)
{
    if (test_checksum())              return 1;
    if (test_valset_encoding())       return 1;
    if (test_valset_limits())         return 1;
    if (test_parser_ack_among_nmea()) return 1;
    if (test_parser_rejects())        return 1;
    printf("OK  %d checks\n", g_checks);
    return 0;
}
