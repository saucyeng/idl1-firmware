/* See idl1_format.h. Pure C99, no ESP-IDF dependency. */

#include "idl1_format.h"

#include <string.h>

#include "idl0_format.h"   /* idl0_format_crc32: the same CRC-32/ISO-HDLC */

static void put_u8(uint8_t *b, size_t *pos, uint8_t v) { b[(*pos)++] = v; }
static void put_u16(uint8_t *b, size_t *pos, uint16_t v)
{
    b[(*pos)++] = (uint8_t)(v & 0xFF);
    b[(*pos)++] = (uint8_t)(v >> 8);
}
static void put_u32(uint8_t *b, size_t *pos, uint32_t v)
{
    for (int i = 0; i < 4; i++) b[(*pos)++] = (uint8_t)(v >> (8 * i));
}
static void put_i64(uint8_t *b, size_t *pos, int64_t v)
{
    uint64_t u = (uint64_t)v;
    for (int i = 0; i < 8; i++) b[(*pos)++] = (uint8_t)(u >> (8 * i));
}
static void put_f32(uint8_t *b, size_t *pos, float v)
{
    uint32_t bits;
    memcpy(&bits, &v, sizeof(bits));
    put_u32(b, pos, bits);
}
/* Copies up to `width - 1` characters of `s` and zero-fills the rest. */
static void put_str(uint8_t *b, size_t *pos, const char *s, size_t width)
{
    memset(b + *pos, 0, width);
    if (s != NULL) {
        size_t n = strlen(s);
        if (n > width - 1) n = width - 1;
        memcpy(b + *pos, s, n);
    }
    *pos += width;
}
static void put_frame(uint8_t *b, size_t *pos, uint8_t type, uint16_t len)
{
    put_u8(b, pos, type);
    put_u16(b, pos, len);
}

size_t idl1_header_size(uint16_t registry_count)
{
    return IDL1_HEADER_FIXED_BYTES + (size_t)registry_count * IDL1_REGISTRY_ENTRY_BYTES + 4;
}

size_t idl1_format_header(uint8_t *out_buf, size_t out_len,
                          const uint8_t session_uuid[16],
                          const uint8_t device_id[6],
                          const char *firmware_version,
                          int64_t session_start_utc_ms,
                          uint32_t config_crc32,
                          const int8_t imu_freq_fine[3],
                          uint16_t registry_count,
                          const idl1_registry_entry_t *registry)
{
    const size_t need = idl1_header_size(registry_count);
    if (out_len < need) return 0;
    size_t pos = 0;
    memcpy(out_buf, "IDL1", 4);
    pos += 4;
    put_u16(out_buf, &pos, IDL1_FORMAT_VERSION);
    put_u32(out_buf, &pos, (uint32_t)need);
    memcpy(out_buf + pos, session_uuid, 16);
    pos += 16;
    memcpy(out_buf + pos, device_id, 6);
    pos += 6;
    put_str(out_buf, &pos, firmware_version, 24);
    put_i64(out_buf, &pos, session_start_utc_ms);
    put_u32(out_buf, &pos, config_crc32);
    for (int i = 0; i < 3; i++) put_u8(out_buf, &pos, (uint8_t)imu_freq_fine[i]);
    put_u16(out_buf, &pos, registry_count);
    for (uint16_t i = 0; i < registry_count; i++) {
        const idl1_registry_entry_t *e = &registry[i];
        put_u16(out_buf, &pos, e->channel_id);
        put_u8(out_buf, &pos, e->data_type);
        put_u8(out_buf, &pos, e->kind);
        put_u8(out_buf, &pos, e->source);
        put_u8(out_buf, &pos, e->axis);
        put_u8(out_buf, &pos, e->path);
        put_u8(out_buf, &pos, 0);   /* reserved */
        put_u32(out_buf, &pos, e->rate_mhz);
        put_f32(out_buf, &pos, e->scale);
        put_f32(out_buf, &pos, e->offset);
        put_str(out_buf, &pos, e->name, 24);
        put_str(out_buf, &pos, e->units, 8);
    }
    put_u32(out_buf, &pos, idl0_format_crc32(out_buf, pos));
    return pos;
}

size_t idl1_imu_block_size(uint16_t n_samples, uint8_t low_axes,
                           uint16_t n_hg, uint8_t hg_axes, uint8_t n_ts)
{
    return IDL1_RECORD_FRAME_BYTES + IDL1_IMU_BLOCK_FIXED_BYTES
         + (size_t)n_samples * low_axes * 2 + (size_t)n_hg * hg_axes * 2
         + (size_t)n_ts * IDL1_TS_WORD_BYTES;
}

size_t idl1_format_imu_block(uint8_t *out_buf, size_t out_len,
                             uint8_t imu_index, uint8_t flags,
                             int64_t read_us, uint32_t read_chip_ts,
                             const int16_t *samples, uint16_t n_samples, uint8_t low_axes,
                             const int16_t *hg, uint16_t n_hg, uint8_t hg_axes,
                             const idl1_ts_word_t *ts, uint8_t n_ts)
{
    const size_t total = idl1_imu_block_size(n_samples, low_axes, n_hg, hg_axes, n_ts);
    const size_t payload = total - IDL1_RECORD_FRAME_BYTES;
    if (out_len < total || payload > 0xFFFF) return 0;
    size_t pos = 0;
    put_frame(out_buf, &pos, IDL1_REC_IMU_BLOCK, (uint16_t)payload);
    put_u8(out_buf, &pos, imu_index);
    put_u8(out_buf, &pos, flags);
    put_i64(out_buf, &pos, read_us);
    put_u32(out_buf, &pos, read_chip_ts);
    put_u16(out_buf, &pos, n_samples);
    put_u16(out_buf, &pos, n_hg);
    put_u8(out_buf, &pos, n_ts);
    /* Raw i16 samples are already little-endian on the ESP32 (and on every
     * host this is tested on): one copy each, no per-sample work. */
    const size_t low_bytes = (size_t)n_samples * low_axes * 2;
    if (low_bytes > 0) memcpy(out_buf + pos, samples, low_bytes);
    pos += low_bytes;
    const size_t hg_bytes = (size_t)n_hg * hg_axes * 2;
    if (hg_bytes > 0) memcpy(out_buf + pos, hg, hg_bytes);
    pos += hg_bytes;
    for (uint8_t i = 0; i < n_ts; i++) {
        put_u16(out_buf, &pos, ts[i].after_sample);
        put_u16(out_buf, &pos, ts[i].after_hg);
        put_u32(out_buf, &pos, ts[i].chip_ts);
    }
    return pos;
}

size_t idl1_format_gps_fix(uint8_t *out_buf,
                           int64_t gps_epoch_ms, int64_t device_timestamp_us,
                           int32_t latitude_e7, int32_t longitude_e7,
                           int16_t altitude_dm, uint16_t speed_kmh_x100,
                           uint16_t heading_x100, uint8_t fix_quality,
                           uint8_t satellites, uint16_t hdop_x100)
{
    size_t pos = 0;
    put_frame(out_buf, &pos, IDL1_REC_GPS_FIX, IDL1_GPS_FIX_PAYLOAD);
    put_i64(out_buf, &pos, gps_epoch_ms);
    put_i64(out_buf, &pos, device_timestamp_us);
    put_u32(out_buf, &pos, (uint32_t)latitude_e7);
    put_u32(out_buf, &pos, (uint32_t)longitude_e7);
    put_u16(out_buf, &pos, (uint16_t)altitude_dm);
    put_u16(out_buf, &pos, speed_kmh_x100);
    put_u16(out_buf, &pos, heading_x100);
    put_u8(out_buf, &pos, fix_quality);
    put_u8(out_buf, &pos, satellites);
    put_u16(out_buf, &pos, hdop_x100);
    return pos;
}

size_t idl1_format_channel_sample(uint8_t *out_buf, uint16_t channel_id,
                                  int64_t timestamp_us,
                                  const void *value, size_t value_len)
{
    if (value_len < 1 || value_len > 8) return 0;
    size_t pos = 0;
    put_frame(out_buf, &pos, IDL1_REC_CHANNEL_SAMPLE, (uint16_t)(2 + 8 + value_len));
    put_u16(out_buf, &pos, channel_id);
    put_i64(out_buf, &pos, timestamp_us);
    memcpy(out_buf + pos, value, value_len);
    return pos + value_len;
}

size_t idl1_format_logger_event(uint8_t *out_buf, uint8_t code,
                                uint16_t source, uint32_t count,
                                int64_t timestamp_us)
{
    size_t pos = 0;
    put_frame(out_buf, &pos, IDL1_REC_LOGGER_EVENT, IDL1_LOGGER_EVENT_PAYLOAD);
    put_u8(out_buf, &pos, code);
    put_u16(out_buf, &pos, source);
    put_u32(out_buf, &pos, count);
    put_i64(out_buf, &pos, timestamp_us);
    return pos;
}

size_t idl1_format_session_summary(uint8_t *out_buf, size_t out_len,
                                   uint8_t n_imu,
                                   const uint32_t *overruns,
                                   const uint32_t *bus_errors,
                                   const uint32_t *samples,
                                   uint32_t writer_drops,
                                   uint32_t slowest_cycle_us)
{
    const size_t payload = 1 + (size_t)n_imu * 12 + 8;
    if (out_len < IDL1_RECORD_FRAME_BYTES + payload) return 0;
    size_t pos = 0;
    put_frame(out_buf, &pos, IDL1_REC_SESSION_SUMMARY, (uint16_t)payload);
    put_u8(out_buf, &pos, n_imu);
    for (uint8_t i = 0; i < n_imu; i++) {
        put_u32(out_buf, &pos, overruns[i]);
        put_u32(out_buf, &pos, bus_errors[i]);
        put_u32(out_buf, &pos, samples[i]);
    }
    put_u32(out_buf, &pos, writer_drops);
    put_u32(out_buf, &pos, slowest_cycle_us);
    return pos;
}

size_t idl1_format_session_end(uint8_t *out_buf)
{
    size_t pos = 0;
    put_frame(out_buf, &pos, IDL1_REC_SESSION_END, 0);
    return pos;
}
