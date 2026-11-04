/* u-blox UBX protocol — see ubx.h. Pure C99 (host-tested). */

#include "ubx.h"

#include <string.h>

void ubx_checksum(const uint8_t *data, size_t len, uint8_t *ck_a, uint8_t *ck_b)
{
    uint8_t a = 0, b = 0;
    for (size_t i = 0; i < len; i++) {
        a = (uint8_t)(a + data[i]);
        b = (uint8_t)(b + a);
    }
    *ck_a = a;
    *ck_b = b;
}

size_t ubx_frame(uint8_t *out, size_t cap, uint8_t cls, uint8_t id,
                 const uint8_t *payload, uint16_t payload_len)
{
    const size_t total = (size_t)payload_len + UBX_FRAME_OVERHEAD;
    if (out == NULL || cap < total) return 0;
    out[0] = UBX_SYNC1;
    out[1] = UBX_SYNC2;
    out[2] = cls;
    out[3] = id;
    out[4] = (uint8_t)(payload_len & 0xFF);
    out[5] = (uint8_t)(payload_len >> 8);
    if (payload_len > 0) memcpy(&out[6], payload, payload_len);
    ubx_checksum(&out[2], (size_t)payload_len + 4, &out[total - 2], &out[total - 1]);
    return total;
}

/* Value size in bytes from key ID bits 28..30 (ID §4.2); 0 = unknown. */
static size_t key_value_size(uint32_t key)
{
    switch ((key >> 28) & 0x7u) {
        case 1: return 1;  /* one bit, stored in one byte */
        case 2: return 1;
        case 3: return 2;
        case 4: return 4;
        case 5: return 8;
        default: return 0;
    }
}

void ubx_valset_init(ubx_valset_t *v, uint8_t layers)
{
    memset(v, 0, sizeof(*v));
    v->buf[0] = 0x00;    /* version 0: applied immediately, no transaction */
    v->buf[1] = layers;
    /* buf[2..3] reserved, zero */
    v->len = 4;
}

bool ubx_valset_add(ubx_valset_t *v, uint32_t key, uint64_t value)
{
    const size_t vsize = key_value_size(key);
    if (vsize == 0 || v->keys >= UBX_VALSET_MAX_KEYS ||
        v->len + 4 + vsize > sizeof(v->buf)) {
        v->overflow = true;
        return false;
    }
    for (int i = 0; i < 4; i++) v->buf[v->len++] = (uint8_t)(key >> (8 * i));
    for (size_t i = 0; i < vsize; i++) v->buf[v->len++] = (uint8_t)(value >> (8 * i));
    v->keys++;
    return true;
}

size_t ubx_valset_frame(const ubx_valset_t *v, uint8_t *out, size_t cap)
{
    if (v->overflow || v->keys == 0) return 0;
    return ubx_frame(out, cap, UBX_CLASS_CFG, UBX_ID_CFG_VALSET, v->buf, (uint16_t)v->len);
}

/* Parser states. */
enum { S_SYNC1 = 0, S_SYNC2, S_CLASS, S_ID, S_LEN1, S_LEN2, S_PAYLOAD, S_CKA, S_CKB };

void ubx_parser_init(ubx_parser_t *p)
{
    memset(p, 0, sizeof(*p));
}

/* Running checksum over class..payload. */
static void ck_add(ubx_parser_t *p, uint8_t byte)
{
    p->ck_a = (uint8_t)(p->ck_a + byte);
    p->ck_b = (uint8_t)(p->ck_b + p->ck_a);
}

bool ubx_parser_feed(ubx_parser_t *p, uint8_t byte)
{
    switch (p->state) {
        case S_SYNC1:
            if (byte == UBX_SYNC1) p->state = S_SYNC2;
            return false;
        case S_SYNC2:
            /* 0xB5 0xB5 0x62: the second 0xB5 may itself start the frame. */
            p->state = (byte == UBX_SYNC2) ? S_CLASS : (byte == UBX_SYNC1 ? S_SYNC2 : S_SYNC1);
            p->ck_a = p->ck_b = 0;
            return false;
        case S_CLASS:
            p->cls = byte; ck_add(p, byte); p->state = S_ID;
            return false;
        case S_ID:
            p->id = byte; ck_add(p, byte); p->state = S_LEN1;
            return false;
        case S_LEN1:
            p->len = byte; ck_add(p, byte); p->state = S_LEN2;
            return false;
        case S_LEN2:
            p->len |= (uint16_t)((uint16_t)byte << 8); ck_add(p, byte);
            p->pos = 0;
            if (p->len > UBX_PARSER_MAX_PAYLOAD) {
                p->state = S_SYNC1;  /* too long to hold: drop and resync */
            } else {
                p->state = (p->len == 0) ? S_CKA : S_PAYLOAD;
            }
            return false;
        case S_PAYLOAD:
            p->payload[p->pos++] = byte; ck_add(p, byte);
            if (p->pos >= p->len) p->state = S_CKA;
            return false;
        case S_CKA:
            p->state = (byte == p->ck_a) ? S_CKB : S_SYNC1;
            return false;
        case S_CKB:
            p->state = S_SYNC1;
            return byte == p->ck_b;
        default:
            p->state = S_SYNC1;
            return false;
    }
}

ubx_ack_t ubx_parser_ack_for(const ubx_parser_t *p, uint8_t cls, uint8_t id)
{
    if (p->cls != UBX_CLASS_ACK || p->len != 2) return UBX_ACK_NONE;
    if (p->payload[0] != cls || p->payload[1] != id) return UBX_ACK_NONE;
    if (p->id == UBX_ID_ACK_ACK) return UBX_ACK_ACK;
    if (p->id == UBX_ID_ACK_NAK) return UBX_ACK_NAK;
    return UBX_ACK_NONE;
}
