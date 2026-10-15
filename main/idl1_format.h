/* .idl1 session file encoders (idl1-app DRAFT spec 2026-10-15,
 * docs/superpowers/specs/2026-10-15-idl1-file-format-DRAFT.md; decision F12).
 *
 * Pure byte-builders, no I/O and no ESP-IDF includes, so the host test
 * builds them with any C compiler. All multi-byte integers little-endian.
 * Records share .idl0's [type:u8][len:u16][payload] framing.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/* Format version written in the header. */
#define IDL1_FORMAT_VERSION 1

/* Header bytes before the registry (spec §4.1): magic 4, format_version 2,
 * header_len 4, session_uuid 16, device_id 6, firmware_version 24,
 * session_start_utc_ms 8, config_crc32 4, imu_freq_fine 3, registry_count 2. */
#define IDL1_HEADER_FIXED_BYTES 73

/* One registry entry (spec §4.2). */
#define IDL1_REGISTRY_ENTRY_BYTES 52

/* Record framing: type + length. */
#define IDL1_RECORD_FRAME_BYTES 3

/* IMU_BLOCK fixed payload (spec §4.4): imu_index 1, flags 1, read_us 8,
 * read_chip_ts 4, n_samples 2, n_hg 2, n_ts 1. */
#define IDL1_IMU_BLOCK_FIXED_BYTES 19

/* One FIFO timestamp word in IMU_BLOCK: after_sample 2, after_hg 2,
 * chip_ts 4. */
#define IDL1_TS_WORD_BYTES 8

/* Record types (spec §4.3). */
#define IDL1_REC_GPS_FIX         0x02
#define IDL1_REC_CHANNEL_SAMPLE  0x03
#define IDL1_REC_IMU_BLOCK       0x10
#define IDL1_REC_LOGGER_EVENT    0x20
#define IDL1_REC_SESSION_SUMMARY 0x21
#define IDL1_REC_SESSION_END     0xFF

/* Registry kinds and IMU paths (spec §4.2). */
#define IDL1_KIND_GENERIC  0
#define IDL1_KIND_IMU_AXIS 1
#define IDL1_KIND_GPS      2
#define IDL1_PATH_LOW_G    0
#define IDL1_PATH_HIGH_G   1

/* Channel id ranges (spec §4.2, decided 2026-10-15). */
#define IDL1_CH_IMU_AXIS_BASE   0   /* 0-17: IMU0..2 x AccelXYZ, GyroXYZ */
#define IDL1_CH_HIGH_G_BASE     18  /* 18-26: IMU0..2 x HgAccelXYZ */
#define IDL1_CH_BOARD_BASE      32  /* 32-47: fixed board inputs */
#define IDL1_CH_WHEEL_FRONT     32
#define IDL1_CH_WHEEL_REAR      33
#define IDL1_CH_PRESSURE_FRONT  34
#define IDL1_CH_PRESSURE_REAR   35
#define IDL1_CH_HR_BPM          36
#define IDL1_CH_HR_RR           37
#define IDL1_CH_USER_BASE       64  /* 64+: user analog/digital, per session */

/* LOGGER_EVENT codes (spec §4.3). */
#define IDL1_EVT_FIFO_OVERRUN  1
#define IDL1_EVT_WRITER_DROP   2
#define IDL1_EVT_BUS_ERROR     3
#define IDL1_EVT_IMU_LOST      4
#define IDL1_EVT_IMU_RECOVERED 5

/* LOGGER_EVENT payload, bytes: code 1, source 2, count 4, timestamp_us 8. */
#define IDL1_LOGGER_EVENT_PAYLOAD 15

/* GPS_FIX payload, bytes: the .idl0 32 plus hdop_x100 2. */
#define IDL1_GPS_FIX_PAYLOAD 34

/* One registry entry, in file field order (spec §4.2). */
typedef struct {
    uint16_t channel_id;
    uint8_t  data_type;    /* 0=u8 1=u16 2=u32 3=i8 4=i16 5=i32 6=f32 7=f64 */
    uint8_t  kind;         /* IDL1_KIND_* */
    uint8_t  source;       /* IMU index for IDL1_KIND_IMU_AXIS, else 0 */
    uint8_t  axis;         /* 0-2 accel X/Y/Z, 3-5 gyro X/Y/Z */
    uint8_t  path;         /* IDL1_PATH_* */
    uint32_t rate_mhz;     /* sample rate, milli-hertz; 0 = event-driven */
    float    scale;        /* physical = raw x scale + offset */
    float    offset;
    char     name[24];     /* NUL-terminated */
    char     units[8];     /* NUL-terminated */
} idl1_registry_entry_t;

/* One FIFO timestamp word placed in its block (spec §4.4). */
typedef struct {
    uint16_t after_sample; /* this block's low-g samples before the word */
    uint16_t after_hg;     /* this block's high-g samples before the word */
    uint32_t chip_ts;      /* the chip's timestamp counter, ticks */
} idl1_ts_word_t;

/* Total header bytes for `registry_count` entries: fixed part, registry,
 * trailing CRC32. */
size_t idl1_header_size(uint16_t registry_count);

/* The file header (spec §4.1), CRC included. `firmware_version` is copied
 * up to 23 characters. Returns bytes written, 0 if `out_len` is short. */
size_t idl1_format_header(uint8_t *out_buf, size_t out_len,
                          const uint8_t session_uuid[16],
                          const uint8_t device_id[6],
                          const char *firmware_version,
                          int64_t session_start_utc_ms,
                          uint32_t config_crc32,
                          const int8_t imu_freq_fine[3],
                          uint16_t registry_count,
                          const idl1_registry_entry_t *registry);

/* IMU_BLOCK size in bytes, framing included. */
size_t idl1_imu_block_size(uint16_t n_samples, uint8_t low_axes,
                           uint16_t n_hg, uint8_t hg_axes, uint8_t n_ts);

/* IMU_BLOCK (spec §4.4). `samples` holds n_samples x low_axes raw values,
 * `hg` n_hg x hg_axes, each packed in axis order. Both are copied with one
 * memcpy each. Returns bytes written, 0 if `out_len` is short or the
 * payload would exceed 65535 bytes. */
size_t idl1_format_imu_block(uint8_t *out_buf, size_t out_len,
                             uint8_t imu_index, uint8_t flags,
                             int64_t read_us, uint32_t read_chip_ts,
                             const int16_t *samples, uint16_t n_samples, uint8_t low_axes,
                             const int16_t *hg, uint16_t n_hg, uint8_t hg_axes,
                             const idl1_ts_word_t *ts, uint8_t n_ts);

/* GPS_FIX (spec §4.3): 37 bytes total. */
size_t idl1_format_gps_fix(uint8_t *out_buf,
                           int64_t gps_epoch_ms, int64_t device_timestamp_us,
                           int32_t latitude_e7, int32_t longitude_e7,
                           int16_t altitude_dm, uint16_t speed_kmh_x100,
                           uint16_t heading_x100, uint8_t fix_quality,
                           uint8_t satellites, uint16_t hdop_x100);

/* CHANNEL_SAMPLE (spec §4.3). value_len 1..8. Returns bytes written. */
size_t idl1_format_channel_sample(uint8_t *out_buf, uint16_t channel_id,
                                  int64_t timestamp_us,
                                  const void *value, size_t value_len);

/* LOGGER_EVENT (spec §4.3): 18 bytes total. */
size_t idl1_format_logger_event(uint8_t *out_buf, uint8_t code,
                                uint16_t source, uint32_t count,
                                int64_t timestamp_us);

/* SESSION_SUMMARY (spec §4.3). The three arrays hold `n_imu` entries.
 * Returns bytes written, 0 if `out_len` is short. */
size_t idl1_format_session_summary(uint8_t *out_buf, size_t out_len,
                                   uint8_t n_imu,
                                   const uint32_t *overruns,
                                   const uint32_t *bus_errors,
                                   const uint32_t *samples,
                                   uint32_t writer_drops,
                                   uint32_t slowest_cycle_us);

/* SESSION_END: 3 bytes. */
size_t idl1_format_session_end(uint8_t *out_buf);
