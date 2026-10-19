/* See tla2528.h. */

#include "tla2528.h"

#include "esp_log.h"

static const char *TAG = "tla2528";

/* I2C transaction timeout, ms. Register accesses are 3-5 bytes at 400 kHz
 * (~0.1 ms); this only bounds a stuck/stretched bus. */
#define TLA2528_TIMEOUT_MS 50

/* Command opcodes (first byte of every register frame). */
#define OP_REG_READ   0x10u   /* S addr/W, 0x10, reg, Sr addr/R, data */
#define OP_REG_WRITE  0x08u   /* S addr/W, 0x08, reg, data, P */

/* Registers used in phase 1. The register map ends at 0x12. */
#define REG_SYSTEM_STATUS  0x00u
#define REG_PIN_CFG        0x05u   /* 1 = channel is GPIO, 0 = analog input */
#define REG_GPIO_CFG       0x07u   /* 1 = digital output, 0 = digital input */
#define REG_GPO_DRIVE_CFG  0x09u   /* 1 = push-pull, 0 = open-drain */
#define REG_GPO_VALUE      0x0Bu   /* output level per GPO */

#define BIT_LED_RED    (1u << TLA2528_CH_LED_RED)
#define BIT_LED_GREEN  (1u << TLA2528_CH_LED_GREEN)
#define BITS_ANALOG    ((1u << TLA2528_CH_ANA_R) | (1u << TLA2528_CH_ANA_F))

/* Channel-role register values. PIN_CFG / GPIO_CFG / GPO_DRIVE_CFG all set
 * only the two LED channels: GPIO, output, push-pull (the LEDs hang off the
 * pins through 330R with no external pull-up, so open-drain would never
 * light them). */
#define LED_CHANNELS_VALUE   (BIT_LED_RED | BIT_LED_GREEN)   /* 0xC0 */

/* The phase-3 sequencer will write AUTO_SEQ_CH_SEL (0x12) = the two analog
 * channels. Defined here so the collision guard below covers it now. */
#define AUTO_SEQ_CH_SEL_VALUE  BITS_ANALOG                   /* 0x30 */

/* Collision guard (pod-firmware-plan.md PT22), checked at compile time
 * because both values are constants: PIN_CFG must never turn AIN4/AIN5 into
 * GPIOs (kills the pod analog inputs), and the sequencer must never scan
 * the LED channels 6/7 (feeds GPO pins into the ADC). */
_Static_assert((LED_CHANNELS_VALUE & BITS_ANALOG) == 0,
               "PIN_CFG would make AIN4/AIN5 GPIOs");
_Static_assert((AUTO_SEQ_CH_SEL_VALUE & (BIT_LED_RED | BIT_LED_GREEN)) == 0,
               "AUTO_SEQ_CH_SEL would scan the LED channels");
_Static_assert((LED_CHANNELS_VALUE & AUTO_SEQ_CH_SEL_VALUE) == 0,
               "a channel cannot be both GPO and sequenced analog input");

static i2c_master_dev_handle_t s_dev = NULL;
static tla2528_probe_t         s_probe;
static uint8_t                 s_gpo_value;   /* shadow of GPO_VALUE */

static esp_err_t reg_read(uint8_t reg, uint8_t *out)
{
    const uint8_t cmd[2] = { OP_REG_READ, reg };
    return i2c_master_transmit_receive(s_dev, cmd, sizeof(cmd), out, 1,
                                       TLA2528_TIMEOUT_MS);
}

static esp_err_t reg_write(uint8_t reg, uint8_t val)
{
    const uint8_t cmd[3] = { OP_REG_WRITE, reg, val };
    return i2c_master_transmit(s_dev, cmd, sizeof(cmd), TLA2528_TIMEOUT_MS);
}

/* Write, read back, and compare one config register. Returns false only on
 * an I2C error; a readback mismatch is logged but tolerated (the phase-1
 * report shows whether the LEDs actually respond). */
static bool write_checked(const char *name, uint8_t reg, uint8_t val)
{
    esp_err_t err = reg_write(reg, val);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "write %s (0x%02X) = 0x%02X failed: %s",
                 name, reg, val, esp_err_to_name(err));
        return false;
    }
    uint8_t rb = 0;
    err = reg_read(reg, &rb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "readback %s (0x%02X) failed: %s",
                 name, reg, esp_err_to_name(err));
        return false;
    }
    if (rb != val) {
        ESP_LOGW(TAG, "%s (0x%02X) wrote 0x%02X, read back 0x%02X",
                 name, reg, val, rb);
    }
    return true;
}

static void release_device(void)
{
    if (s_dev != NULL) {
        i2c_master_bus_rm_device(s_dev);
        s_dev = NULL;
    }
}

bool tla2528_init(i2c_master_bus_handle_t bus, uint8_t addr)
{
    release_device();
    s_probe = (tla2528_probe_t){ .probed = true, .addr = addr };
    s_gpo_value = 0;

    if (bus == NULL) {
        ESP_LOGE(TAG, "TLA2528 NOT PROBED at 0x%02X: no I2C bus — "
                      "analog inputs and board LEDs disabled this boot", addr);
        return false;
    }

    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = addr,
        .scl_speed_hz    = TLA2528_SCL_HZ,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TLA2528 NOT PROBED at 0x%02X: bus_add_device: %s — "
                      "analog inputs and board LEDs disabled this boot",
                 addr, esp_err_to_name(err));
        s_dev = NULL;
        return false;
    }

    uint8_t st = 0;
    err = reg_read(REG_SYSTEM_STATUS, &st);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "*** TLA2528 NOT RESPONDING at I2C 0x%02X (%s) — "
                      "analog inputs and board LEDs (red/green) disabled this "
                      "boot; onboard LED only. See docs/bringup.md (ADDR / "
                      "NT1). ***", addr, esp_err_to_name(err));
        release_device();
        return false;
    }
    s_probe.acked         = true;
    s_probe.system_status = st;
    s_probe.status_id_ok  =
        (st & TLA2528_SYSTEM_STATUS_ID_MASK) == TLA2528_SYSTEM_STATUS_ID_VALUE;
    if (s_probe.status_id_ok) {
        ESP_LOGI(TAG, "TLA2528 ACK at 0x%02X, SYSTEM_STATUS=0x%02X", addr, st);
    } else {
        ESP_LOGW(TAG, "device ACKed at 0x%02X but SYSTEM_STATUS=0x%02X fails "
                      "the TLA2528 reserved-bit check (mask 0x%02X, want 0x%02X)"
                      " — is this really the TLA2528?",
                 addr, st, TLA2528_SYSTEM_STATUS_ID_MASK,
                 TLA2528_SYSTEM_STATUS_ID_VALUE);
    }

    /* Output level first (both LEDs off), then pin role, drive mode, and
     * direction last, so neither LED channel ever drives before its level
     * is defined. */
    if (!write_checked("GPO_VALUE",     REG_GPO_VALUE,     0x00) ||
        !write_checked("PIN_CFG",       REG_PIN_CFG,       LED_CHANNELS_VALUE) ||
        !write_checked("GPO_DRIVE_CFG", REG_GPO_DRIVE_CFG, LED_CHANNELS_VALUE) ||
        !write_checked("GPIO_CFG",      REG_GPIO_CFG,      LED_CHANNELS_VALUE)) {
        ESP_LOGE(TAG, "TLA2528 at 0x%02X ACKed but LED config failed — "
                      "board LEDs disabled this boot", addr);
        release_device();
        return false;
    }

    s_probe.configured = true;
    ESP_LOGI(TAG, "TLA2528 up: ch%d/ch%d push-pull GPO (LEDs), ch%d/ch%d analog",
             TLA2528_CH_LED_RED, TLA2528_CH_LED_GREEN,
             TLA2528_CH_ANA_R, TLA2528_CH_ANA_F);
    return true;
}

bool tla2528_is_up(void)
{
    return s_probe.configured;
}

bool tla2528_led_set(bool red_on, bool green_on)
{
    if (!s_probe.configured) return false;
    uint8_t v = s_gpo_value & (uint8_t)~(BIT_LED_RED | BIT_LED_GREEN);
    if (red_on)   v |= BIT_LED_RED;
    if (green_on) v |= BIT_LED_GREEN;
    esp_err_t err = reg_write(REG_GPO_VALUE, v);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "GPO_VALUE write failed: %s", esp_err_to_name(err));
        return false;
    }
    s_gpo_value = v;
    return true;
}

void tla2528_get_probe(tla2528_probe_t *out)
{
    if (out == NULL) return;
    *out = s_probe;
}
