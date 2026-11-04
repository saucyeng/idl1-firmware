/* u-blox UBX protocol — frame builder, CFG-VALSET encoder, and a byte-at-
 * a-time frame parser for the SAM-M10Q (u-blox M10 SPG 5.10, protocol
 * 34.10; Interface Description UBX-21035062 R03).
 *
 * Pure C99, no ESP-IDF includes: host-tested by test/test_ubx.c. The UART
 * side (send, wait for UBX-ACK) lives in gps_driver.c.
 *
 * Frame (ID §3.2): 0xB5 0x62, class u8, id u8, length u16 LE, payload,
 * CK_A, CK_B — an 8-bit Fletcher checksum over class..payload (ID §3.4). */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UBX_SYNC1             0xB5
#define UBX_SYNC2             0x62
#define UBX_FRAME_OVERHEAD    8     /* bytes: sync 2 + class/id 2 + length 2 + checksum 2 */

#define UBX_CLASS_ACK         0x05
#define UBX_ID_ACK_NAK        0x00
#define UBX_ID_ACK_ACK        0x01
#define UBX_CLASS_CFG         0x06
#define UBX_ID_CFG_VALSET     0x8A
#define UBX_CLASS_MON         0x0A
#define UBX_ID_MON_VER        0x04

/* UBX-CFG-VALSET layers bitfield (ID §3.10.5). RAM only: the firmware
 * re-applies its config at every boot, so nothing is saved to BBR/flash
 * (a module stuck on a bad saved config could not be recovered — RESET_N
 * and SAFEBOOT_N are unconnected on IDL1). */
#define UBX_LAYER_RAM         0x01

/* At most 64 key/value pairs per VALSET (ID §3.10.5). */
#define UBX_VALSET_MAX_KEYS   64

/* --- Configuration keys used by the firmware (ID §4.9, Table per group).
 * Bits 28..30 of a key ID give its value size (ID §4.2): 1 = one bit
 * (sent as one byte), 2 = 1 byte, 3 = 2 bytes, 4 = 4 bytes, 5 = 8 bytes. */
#define UBX_CFG_RATE_MEAS               0x30210001u  /* U2, ms between measurements; min 25 */
#define UBX_CFG_RATE_NAV                0x30210002u  /* U2, measurements per solution; 1..127 */
#define UBX_CFG_UART1_BAUDRATE          0x40520001u  /* U4, bit/s */
#define UBX_CFG_UART1OUTPROT_NMEA       0x10740002u  /* L */
#define UBX_CFG_UART1OUTPROT_UBX        0x10740001u  /* L */
#define UBX_CFG_MSGOUT_NMEA_GGA_UART1   0x209100BBu  /* U1, output per navigation solution; 0 = off */
#define UBX_CFG_MSGOUT_NMEA_GLL_UART1   0x209100CAu
#define UBX_CFG_MSGOUT_NMEA_GSA_UART1   0x209100C0u
#define UBX_CFG_MSGOUT_NMEA_GSV_UART1   0x209100C5u
#define UBX_CFG_MSGOUT_NMEA_RMC_UART1   0x209100ACu
#define UBX_CFG_MSGOUT_NMEA_VTG_UART1   0x209100B1u
#define UBX_CFG_MSGOUT_NMEA_ZDA_UART1   0x209100D9u
#define UBX_CFG_NAVSPG_DYNMODEL         0x20110021u  /* E1, see UBX_DYNMODEL_* */
#define UBX_CFG_SIGNAL_SBAS_ENA         0x10310020u  /* L; a CFG-SIGNAL change restarts the GNSS subsystem */

/* CFG-NAVSPG-DYNMODEL values (ID Table 22). WRIST/BIKE/MOWER/ESCOOTER are
 * "not available in all products" and are not offered. */
#define UBX_DYNMODEL_PORTABLE     0
#define UBX_DYNMODEL_STATIONARY   2
#define UBX_DYNMODEL_PEDESTRIAN   3
#define UBX_DYNMODEL_AUTOMOTIVE   4
#define UBX_DYNMODEL_SEA          5
#define UBX_DYNMODEL_AIRBORNE_1G  6
#define UBX_DYNMODEL_AIRBORNE_2G  7
#define UBX_DYNMODEL_AIRBORNE_4G  8

/* Fletcher-8 over `len` bytes (class..payload), ID §3.4. */
void ubx_checksum(const uint8_t *data, size_t len, uint8_t *ck_a, uint8_t *ck_b);

/* Writes a complete frame into `out` (capacity `cap` bytes). Returns the
 * frame length in bytes, or 0 if it does not fit. */
size_t ubx_frame(uint8_t *out, size_t cap, uint8_t cls, uint8_t id,
                 const uint8_t *payload, uint16_t payload_len);

/* CFG-VALSET payload under construction: init, add keys, then frame it
 * with ubx_valset_frame. `buf` holds the payload (header + key/values). */
typedef struct {
    uint8_t  buf[4 + UBX_VALSET_MAX_KEYS * 12];  /* bytes: header 4 + up to 64 × (key 4 + value ≤ 8) */
    size_t   len;
    unsigned keys;
    bool     overflow;  /* a key was rejected: too many, unknown size, or out of room */
} ubx_valset_t;

void ubx_valset_init(ubx_valset_t *v, uint8_t layers);

/* Appends key = value, sized from the key ID. Values wider than the key
 * are truncated to its size, little-endian. Returns false (and sets
 * `overflow`) when the key cannot be added. */
bool ubx_valset_add(ubx_valset_t *v, uint32_t key, uint64_t value);

/* Frames the VALSET into `out`. Returns 0 on overflow or no room. */
size_t ubx_valset_frame(const ubx_valset_t *v, uint8_t *out, size_t cap);

/* Byte-at-a-time frame parser. Bytes outside a UBX frame (NMEA text) are
 * skipped. A frame whose payload is longer than UBX_PARSER_MAX_PAYLOAD is
 * dropped, as is one with a bad checksum. */
#define UBX_PARSER_MAX_PAYLOAD 256  /* bytes; MON-VER is 40 + n·30 */

typedef struct {
    uint8_t  state;
    uint8_t  cls, id;
    uint16_t len, pos;
    uint8_t  ck_a, ck_b;
    uint8_t  payload[UBX_PARSER_MAX_PAYLOAD];
} ubx_parser_t;

void ubx_parser_init(ubx_parser_t *p);

/* Feeds one byte. Returns true when it completes a valid frame; the frame
 * is then readable in p->cls, p->id, p->len and p->payload until the next
 * call. */
bool ubx_parser_feed(ubx_parser_t *p, uint8_t byte);

/* Result of matching a parsed frame against an outstanding command. */
typedef enum {
    UBX_ACK_NONE = 0,  /* not an ACK for (cls, id) */
    UBX_ACK_ACK,       /* UBX-ACK-ACK for (cls, id) */
    UBX_ACK_NAK,       /* UBX-ACK-NAK for (cls, id) */
} ubx_ack_t;

ubx_ack_t ubx_parser_ack_for(const ubx_parser_t *p, uint8_t cls, uint8_t id);
