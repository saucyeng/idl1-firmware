/* See i2c_bus.h. */

#include "i2c_bus.h"

#include "esp_log.h"

#include "pins.h"

static const char *TAG = "i2c_bus";

static i2c_master_bus_handle_t s_bus = NULL;

i2c_master_bus_handle_t idl1_i2c_bus_get(void)
{
    if (s_bus != NULL) return s_bus;

    const i2c_master_bus_config_t cfg = {
        .i2c_port          = I2C_NUM_0,
        .sda_io_num        = IDL1_PIN_I2C_SDA,
        .scl_io_num        = IDL1_PIN_I2C_SCL,
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,     /* APB cycles; IDF-recommended default */
        /* R8/R9 4k7 pull-ups are on the board; the ~45k internals would
         * only add an uncontrolled parallel path. */
        .flags.enable_internal_pullup = false,
    };
    i2c_master_bus_handle_t bus = NULL;
    esp_err_t err = i2c_new_master_bus(&cfg, &bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus: %s", esp_err_to_name(err));
        return NULL;
    }
    s_bus = bus;
    ESP_LOGI(TAG, "I2C bus up: SDA=GPIO%d SCL=GPIO%d, devices @ %d Hz",
             IDL1_PIN_I2C_SDA, IDL1_PIN_I2C_SCL, IDL1_I2C_SCL_HZ);
    return s_bus;
}
