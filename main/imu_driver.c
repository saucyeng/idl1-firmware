/* LSM6DSV320X SPI bus adapter — the onboard IMU0.
 *
 * Keeps only what is SPI-specific: the device on the shared SPI2_HOST (its
 * chip-select from pins.h), the ST platform read/write/delay glue, and the
 * public idl0_imu_* wrappers. Every register-level operation is in
 * imu_chip_dsv320x.c, shared with the pod (I2C) adapter.
 */

#include "imu_driver.h"

#include <string.h>

#include "driver/spi_master.h"
#include "esp_attr.h"          /* WORD_ALIGNED_ATTR for the DMA burst buffers */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "imu_chip_dsv320x.h"
#include "pins.h"

static const char *TAG = "imu";

/* SPI clock for the IMU, Hz. The DSV320X allows 10 MHz; the bus carries
 * only IMU0 and the SD card, both on short board traces. */
#define IMU_SPI_HZ        (10 * 1000 * 1000)
#define IMU_QUEUE_DEPTH   1

/* Short-transfer scratch size, bytes: register accesses at init. Longer
 * reads (the FIFO burst) go through the DMA burst buffers below. */
#define IMU_SPI_SMALL_MAX 32

/* CS pin per IMU index (IMU0 onboard sprung mass; IMU1 front pod; IMU2 rear
 * pod). On IDL1 only IMU0 is on SPI: the pod IMUs are I2C devices
 * (imu_driver_i2c.c, addresses in pins.h) and have no chip-select, so their
 * slots are GPIO_NUM_NC and idl0_imu_init_one() refuses them. */
static const gpio_num_t s_cs_pin[IDL0_IMU_COUNT] = {
    IDL1_PIN_CS_IMU0,
    GPIO_NUM_NC,
    GPIO_NUM_NC,
};

/* Per-IMU runtime state. NULL spi handle == no device on the bus. */
static spi_device_handle_t s_spi[IDL0_IMU_COUNT];
static stmdev_ctx_t        s_ctx[IDL0_IMU_COUNT];
static imu_chip_instance_t s_chip[IDL0_IMU_COUNT];
static bool                s_init_attempted[IDL0_IMU_COUNT];

/* DMA-capable, word-aligned scratch for the FIFO burst (1 address byte +
 * up to IMU_CHIP_BURST_MAX_WORDS x 7 data bytes). The +4 pads for the SPI
 * DMA engine rounding the rx length up to a 4-byte multiple. Only the IMU
 * task issues long reads, one IMU at a time, so one pair is enough. Static:
 * 2 x ~3.5 KB would blow the task stack. */
#define IMU_SPI_BURST_BUF_BYTES (1 + IMU_CHIP_BURST_MAX_WORDS * 7 + 4)
static WORD_ALIGNED_ATTR uint8_t s_burst_tx[IMU_SPI_BURST_BUF_BYTES];
static WORD_ALIGNED_ATTR uint8_t s_burst_rx[IMU_SPI_BURST_BUF_BYTES];

/* --- ST driver platform glue ----------------------------------------------
 * `handle` points at the per-IMU spi_device_handle_t, so one set of glue
 * functions serves every SPI IMU. */

static int32_t platform_write(void *handle, uint8_t reg,
                              const uint8_t *bufp, uint16_t len)
{
    spi_device_handle_t *spi = (spi_device_handle_t *)handle;
    uint8_t tx[1 + IMU_SPI_SMALL_MAX];
    if (len > IMU_SPI_SMALL_MAX) return -1;
    tx[0] = reg & 0x7F;  /* MSB=0 -> write */
    memcpy(tx + 1, bufp, len);
    spi_transaction_t t = {
        .length    = (1 + len) * 8,
        .tx_buffer = tx,
    };
    return (spi_device_transmit(*spi, &t) == ESP_OK) ? 0 : -1;
}

static int32_t platform_read(void *handle, uint8_t reg,
                             uint8_t *bufp, uint16_t len)
{
    spi_device_handle_t *spi = (spi_device_handle_t *)handle;
    if (len <= IMU_SPI_SMALL_MAX) {
        uint8_t tx[1 + IMU_SPI_SMALL_MAX] = {0};
        uint8_t rx[1 + IMU_SPI_SMALL_MAX] = {0};
        tx[0] = reg | 0x80;  /* MSB=1 -> read */
        spi_transaction_t t = {
            .length    = (1 + len) * 8,
            .tx_buffer = tx,
            .rx_buffer = rx,
        };
        if (spi_device_transmit(*spi, &t) != ESP_OK) return -1;
        memcpy(bufp, rx + 1, len);
        return 0;
    }
    /* FIFO burst: one full-duplex DMA transaction. */
    if ((size_t)len + 1 > IMU_SPI_BURST_BUF_BYTES - 4) return -1;
    s_burst_tx[0] = reg | 0x80;
    memset(s_burst_tx + 1, 0, len);
    spi_transaction_t t = {
        .length    = (1 + (size_t)len) * 8,
        .tx_buffer = s_burst_tx,
        .rx_buffer = s_burst_rx,
    };
    if (spi_device_transmit(*spi, &t) != ESP_OK) return -1;
    memcpy(bufp, s_burst_rx + 1, len);
    return 0;
}

static void platform_mdelay(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/* Hand an IMU chip-select back to plain GPIO, driven high (deasserted).
 * spi_bus_remove_device() only disables the pad's output, which would leave
 * the CS floating: a DSV320X that reads it as low joins SD traffic on the
 * shared bus and can drive MISO. Same reasoning as app_main's CS pre-idle. */
static void spi_cs_park(gpio_num_t cs)
{
    gpio_set_level(cs, 1);
    gpio_set_direction(cs, GPIO_MODE_OUTPUT);
}

/* --- Public API ------------------------------------------------------------ */

const char *idl0_imu_state_str(idl0_imu_state_t state)
{
    switch (state) {
        case IDL0_IMU_OK:      return "OK";
        case IDL0_IMU_PARTIAL: return "PARTIAL";
        case IDL0_IMU_ERROR:   return "ERROR";
        case IDL0_IMU_ABSENT:
        default:               return "ABSENT";
    }
}

/* T32: aggregates only this (SPI) adapter's IMUs; idl0_imu_task_state()
 * (imu_task.c) is the whole-device aggregate status.c reports. */
idl0_imu_state_t idl0_imu_state(void)
{
    int attempted = 0, ok_count = 0, error = 0;
    for (int i = 0; i < IDL0_IMU_COUNT; i++) {
        if (!s_init_attempted[i]) continue;
        attempted++;
        if (s_chip[i].per_state == IDL0_IMU_OK)         ok_count++;
        else if (s_chip[i].per_state == IDL0_IMU_ERROR) error++;
    }
    if (attempted == 0) return IDL0_IMU_ABSENT;
    if (error > 0)      return IDL0_IMU_ERROR;
    if (ok_count == 0)  return IDL0_IMU_ABSENT;
    if (ok_count < attempted) return IDL0_IMU_PARTIAL;
    return IDL0_IMU_OK;
}

bool idl0_imu_is_active(uint8_t imu_index)
{
    if (imu_index >= IDL0_IMU_COUNT) return false;
    return s_chip[imu_index].per_state == IDL0_IMU_OK;
}

idl0_imu_state_t idl0_imu_per_state(uint8_t imu_index)
{
    if (imu_index >= IDL0_IMU_COUNT) return IDL0_IMU_ABSENT;
    return s_chip[imu_index].per_state;
}

bool idl0_imu_init_one(spi_host_device_t spi_host,
                       uint8_t imu_index,
                       uint16_t sample_rate_hz,
                       uint8_t accel_range_g,
                       uint16_t gyro_range_dps,
                       bool high_performance)
{
    if (imu_index >= IDL0_IMU_COUNT) {
        ESP_LOGE(TAG, "init_one: index %u out of range", (unsigned)imu_index);
        return false;
    }
    if (s_cs_pin[imu_index] == GPIO_NUM_NC) {
        /* IDL1: IMU1/IMU2 are I2C pods. Reaching here is a bus-map bug in the
         * caller (imu_task.c s_imu_bus[]), not a missing chip. */
        ESP_LOGE(TAG, "init_one: IMU%u has no SPI chip-select on IDL1 (I2C pod)",
                 (unsigned)imu_index);
        return false;
    }

    s_init_attempted[imu_index] = true;
    s_chip[imu_index].per_state = IDL0_IMU_ABSENT;
    s_chip[imu_index].probe = (idl0_imu_probe_t){ .status = IDL0_IMU_PROBE_NOT_RUN };

    /* T31 re-runs init at session start to recover a live-faulted IMU; a
     * prior attempt may have left the device attached. Release it first so
     * repeated init attempts don't leak a device slot. Safe without a lock
     * only because of the cross-task invariant at
     * idl0_imu_task_reinit_failed() (imu_task.h). */
    if (s_spi[imu_index] != NULL) {
        spi_bus_remove_device(s_spi[imu_index]);
        s_spi[imu_index] = NULL;
    }

    spi_device_interface_config_t dev = {
        .clock_speed_hz = IMU_SPI_HZ,
        .mode           = 0,                 /* CPOL=0, CPHA=0 */
        .spics_io_num   = s_cs_pin[imu_index],
        .queue_size     = IMU_QUEUE_DEPTH,
    };
    esp_err_t err = spi_bus_add_device(spi_host, &dev, &s_spi[imu_index]);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "IMU%u spi_bus_add_device: %s",
                 (unsigned)imu_index, esp_err_to_name(err));
        s_chip[imu_index].probe.status = IDL0_IMU_PROBE_NO_RESPONSE;
        return false;
    }

    stmdev_ctx_t *ctx = &s_ctx[imu_index];
    ctx->write_reg = platform_write;
    ctx->read_reg  = platform_read;
    ctx->mdelay    = platform_mdelay;
    ctx->handle    = &s_spi[imu_index];

    /* The onboard IMU never carries high-g (pods only, registry 24-29). */
    const bool ok = imu_chip_init(ctx, &s_chip[imu_index], imu_index, true,
                                  sample_rate_hz, accel_range_g, gyro_range_dps,
                                  high_performance, false, 0, 0);
    if (!ok && s_chip[imu_index].per_state == IDL0_IMU_ABSENT) {
        /* Not a DSV320X / no answer: leave the bus to the SD card. An ERROR
         * (config write failed) keeps the device for the T31 retry. */
        spi_bus_remove_device(s_spi[imu_index]);
        s_spi[imu_index] = NULL;
        spi_cs_park(s_cs_pin[imu_index]);
    }
    return ok;
}

idl0_imu_probe_t idl0_imu_probe_result(uint8_t imu_index)
{
    if (imu_index >= IDL0_IMU_COUNT) {
        return (idl0_imu_probe_t){ .status = IDL0_IMU_PROBE_NOT_RUN };
    }
    return s_chip[imu_index].probe;
}

uint16_t idl0_imu_fifo_count(uint8_t imu_index)
{
    if (imu_index >= IDL0_IMU_COUNT) return 0;
    if (s_chip[imu_index].per_state != IDL0_IMU_OK) return 0;
    return imu_chip_fifo_count(&s_ctx[imu_index], &s_chip[imu_index]);
}

size_t idl0_imu_drain(uint8_t imu_index, idl0_imu_sample_t *out, size_t max_pairs)
{
    if (imu_index >= IDL0_IMU_COUNT) return 0;
    return imu_chip_drain(&s_ctx[imu_index], &s_chip[imu_index],
                          out, max_pairs, NULL, 0, NULL);
}

bool idl0_imu_check_overrun(uint8_t imu_index)
{
    if (imu_index >= IDL0_IMU_COUNT) return false;
    return imu_chip_check_overrun(&s_chip[imu_index]);
}

void idl0_imu_take_pair_diag(uint8_t imu_index, uint32_t *reuse, uint32_t *unknown)
{
    if (imu_index >= IDL0_IMU_COUNT) {
        if (reuse)   *reuse   = 0;
        if (unknown) *unknown = 0;
        return;
    }
    imu_chip_take_pair_diag(&s_chip[imu_index], reuse, unknown);
}
