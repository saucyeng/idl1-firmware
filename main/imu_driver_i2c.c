/* LSM6DSV320X I2C bus adapter — the two pod IMUs. See imu_driver_i2c.h.
 *
 * Keeps only what is I2C-specific: the device on the shared IDL1 I2C bus
 * (i2c_bus.c), the ST platform read/write/delay glue, and the public
 * idl0_imu_*_i2c wrappers. Every register-level operation is in
 * imu_chip_dsv320x.c, shared with the onboard (SPI) adapter.
 */

#include "imu_driver_i2c.h"

#include <string.h>

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "i2c_bus.h"
#include "imu_chip_dsv320x.h"

static const char *TAG = "imu_i2c";

/* Per-transaction timeout, ms. A full 512-word FIFO burst is ~3.6 KB,
 * about 90 ms at 400 kHz; this bounds a stuck bus, not a normal read. */
#define I2C_TIMEOUT_MS     200

/* Longest register write the glue supports, bytes (init writes are 1-2). */
#define I2C_WRITE_MAX      32

typedef struct {
    i2c_master_dev_handle_t dev;
} i2c_imu_t;

static i2c_imu_t           s_i2c[IDL0_IMU_COUNT];
static stmdev_ctx_t        s_ctx[IDL0_IMU_COUNT];
static imu_chip_instance_t s_chip[IDL0_IMU_COUNT];
/* T31/T32: was idl0_imu_init_one_i2c() ever called for this index? Lets
 * imu_task.c tell "never configured" from "configured but currently down". */
static bool                s_init_attempted[IDL0_IMU_COUNT];

/* --- ST driver platform glue ----------------------------------------------
 * Write = [reg, data...]; read = [reg] then repeated-start read. */

static int32_t platform_write(void *handle, uint8_t reg,
                              const uint8_t *bufp, uint16_t len)
{
    i2c_imu_t *imu = (i2c_imu_t *)handle;
    uint8_t buf[1 + I2C_WRITE_MAX];
    if (len > I2C_WRITE_MAX) return -1;
    buf[0] = reg;
    memcpy(buf + 1, bufp, len);
    esp_err_t err = i2c_master_transmit(imu->dev, buf, 1 + len, I2C_TIMEOUT_MS);
    return (err == ESP_OK) ? 0 : -1;
}

static int32_t platform_read(void *handle, uint8_t reg,
                             uint8_t *bufp, uint16_t len)
{
    i2c_imu_t *imu = (i2c_imu_t *)handle;
    esp_err_t err = i2c_master_transmit_receive(imu->dev, &reg, 1, bufp, len,
                                                I2C_TIMEOUT_MS);
    return (err == ESP_OK) ? 0 : -1;
}

static void platform_mdelay(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static void release_device(uint8_t imu_index)
{
    if (s_i2c[imu_index].dev != NULL) {
        i2c_master_bus_rm_device(s_i2c[imu_index].dev);
        s_i2c[imu_index].dev = NULL;
    }
}

/* --- Public API ------------------------------------------------------------ */

bool idl0_imu_init_one_i2c(uint8_t imu_index,
                           uint8_t i2c_addr,
                           uint16_t sample_rate_hz,
                           uint8_t accel_range_g,
                           uint16_t gyro_range_dps,
                           bool high_performance)
{
    if (imu_index >= IDL0_IMU_COUNT) {
        ESP_LOGE(TAG, "init_one_i2c: index %u out of range", (unsigned)imu_index);
        return false;
    }
    s_init_attempted[imu_index] = true;
    s_chip[imu_index].per_state = IDL0_IMU_ABSENT;
    s_chip[imu_index].probe = (idl0_imu_probe_t){ .status = IDL0_IMU_PROBE_NO_RESPONSE };

    /* T31 re-runs init at session start; release any handle a previous
     * attempt left attached so retries don't leak device slots. Safe without
     * a lock only because of the cross-task invariant at
     * idl0_imu_task_reinit_failed() (imu_task.h). */
    release_device(imu_index);

    i2c_master_bus_handle_t bus = idl1_i2c_bus_get();
    if (bus == NULL) return false;

    /* Address-only probe of this one address first (not a scan): an
     * unplugged pod is a normal configuration (P7-H3), and probing avoids
     * the I2C driver's NACK error log that a register read would trigger. */
    esp_err_t err = i2c_master_probe(bus, i2c_addr, I2C_TIMEOUT_MS);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NOT_FOUND) {
            ESP_LOGI(TAG, "IMU%u pod absent: no ACK at I2C 0x%02X",
                     (unsigned)imu_index, i2c_addr);
        } else {
            ESP_LOGW(TAG, "IMU%u probe at I2C 0x%02X: %s (bus stuck / pull-ups?)",
                     (unsigned)imu_index, i2c_addr, esp_err_to_name(err));
        }
        return false;
    }

    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = i2c_addr,
        .scl_speed_hz    = IDL1_I2C_SCL_HZ,
    };
    err = i2c_master_bus_add_device(bus, &dev_cfg, &s_i2c[imu_index].dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "IMU%u(I2C@0x%02X) bus_add_device: %s",
                 (unsigned)imu_index, i2c_addr, esp_err_to_name(err));
        s_i2c[imu_index].dev = NULL;
        return false;
    }

    stmdev_ctx_t *ctx = &s_ctx[imu_index];
    ctx->write_reg = platform_write;
    ctx->read_reg  = platform_read;
    ctx->mdelay    = platform_mdelay;
    ctx->handle    = &s_i2c[imu_index];

    /* High-g is bring-up phase 3: off for now. */
    const bool ok = imu_chip_init(ctx, &s_chip[imu_index], imu_index, false,
                                  sample_rate_hz, accel_range_g, gyro_range_dps,
                                  high_performance, false, 0, 0);
    if (!ok && s_chip[imu_index].per_state == IDL0_IMU_ABSENT) {
        release_device(imu_index);
    }
    return ok;
}

bool idl0_imu_is_active_i2c(uint8_t imu_index)
{
    if (imu_index >= IDL0_IMU_COUNT) return false;
    return s_chip[imu_index].per_state == IDL0_IMU_OK;
}

bool idl0_imu_i2c_init_attempted(uint8_t imu_index)
{
    if (imu_index >= IDL0_IMU_COUNT) return false;
    return s_init_attempted[imu_index];
}

idl0_imu_probe_t idl0_imu_probe_result_i2c(uint8_t imu_index)
{
    if (imu_index >= IDL0_IMU_COUNT || !s_init_attempted[imu_index]) {
        return (idl0_imu_probe_t){ .status = IDL0_IMU_PROBE_NOT_RUN };
    }
    return s_chip[imu_index].probe;
}

uint16_t idl0_imu_fifo_count_i2c(uint8_t imu_index)
{
    if (imu_index >= IDL0_IMU_COUNT) return 0;
    if (s_chip[imu_index].per_state != IDL0_IMU_OK) return 0;
    return imu_chip_fifo_count(&s_ctx[imu_index], &s_chip[imu_index]);
}

bool idl0_imu_check_overrun_i2c(uint8_t imu_index)
{
    if (imu_index >= IDL0_IMU_COUNT) return false;
    return imu_chip_check_overrun(&s_chip[imu_index]);
}

void idl0_imu_take_pair_diag_i2c(uint8_t imu_index, uint32_t *reuse, uint32_t *unknown)
{
    if (imu_index >= IDL0_IMU_COUNT) {
        if (reuse)   *reuse   = 0;
        if (unknown) *unknown = 0;
        return;
    }
    imu_chip_take_pair_diag(&s_chip[imu_index], reuse, unknown);
}

size_t idl0_imu_drain_i2c(uint8_t imu_index, idl0_imu_sample_t *out,
                          size_t max_pairs)
{
    if (imu_index >= IDL0_IMU_COUNT) return 0;
    return imu_chip_drain(&s_ctx[imu_index], &s_chip[imu_index],
                          out, max_pairs, NULL, 0, NULL);
}
