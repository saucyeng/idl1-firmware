#include "battery.h"

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "pins.h"

/* Samples averaged per reading. */
#define BAT_SAMPLES 16

static SemaphoreHandle_t         s_lock  = NULL;
static portMUX_TYPE              s_init_mux = portMUX_INITIALIZER_UNLOCKED;
static adc_oneshot_unit_handle_t s_adc   = NULL;
static adc_channel_t             s_chan;
static adc_cali_handle_t         s_cali  = NULL;
static const char               *s_init_error = NULL;

/* Creates the ADC unit, channel config and calibration once. Called with
 * s_lock held. */
static bool ensure_init(const char **why)
{
    if (s_adc != NULL) return true;
    if (s_init_error != NULL) {
        *why = s_init_error;
        return false;
    }
    adc_unit_t unit;
    if (adc_oneshot_io_to_channel(IDL1_PIN_BAT_SENSE, &unit, &s_chan) != ESP_OK) {
        s_init_error = "GPIO is not an ADC pin";
        *why = s_init_error;
        return false;
    }
    const adc_oneshot_unit_init_cfg_t ucfg = { .unit_id = unit };
    adc_oneshot_unit_handle_t adc = NULL;
    if (adc_oneshot_new_unit(&ucfg, &adc) != ESP_OK) {
        *why = "adc_oneshot_new_unit failed";
        return false;  /* retried next call */
    }
    /* 12 dB attenuation: full scale ~3.1 V at the pin, above the 2.8 V a
     * full cell puts there. */
    const adc_oneshot_chan_cfg_t ccfg = {
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_oneshot_config_channel(adc, s_chan, &ccfg) != ESP_OK) {
        adc_oneshot_del_unit(adc);
        *why = "adc_oneshot_config_channel failed";
        return false;
    }
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    const adc_cali_curve_fitting_config_t kcfg = {
        .unit_id  = unit,
        .chan     = s_chan,
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&kcfg, &s_cali) != ESP_OK) {
        s_cali = NULL;
    }
#endif
    s_adc = adc;
    return true;
}

bool idl1_battery_read(int *raw_avg, int *pin_mv, int *vbat_mv, const char **why)
{
    if (s_lock == NULL) {
        SemaphoreHandle_t lock = xSemaphoreCreateMutex();
        portENTER_CRITICAL(&s_init_mux);
        if (s_lock == NULL) {
            s_lock = lock;
            lock = NULL;
        }
        portEXIT_CRITICAL(&s_init_mux);
        if (lock != NULL) vSemaphoreDelete(lock);
        if (s_lock == NULL) {
            *why = "no memory for the battery lock";
            return false;
        }
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool ok = false;
    if (!ensure_init(why)) goto out;

    int32_t sum = 0;
    for (int i = 0; i < BAT_SAMPLES; i++) {
        int raw = 0;
        if (adc_oneshot_read(s_adc, s_chan, &raw) != ESP_OK) {
            *why = "adc_oneshot_read failed";
            goto out;
        }
        sum += raw;
    }
    *raw_avg = (int)(sum / BAT_SAMPLES);
    if (s_cali == NULL) {
        *why = "no ADC calibration (eFuse not burnt?)";
        goto out;
    }
    if (adc_cali_raw_to_voltage(s_cali, *raw_avg, pin_mv) != ESP_OK) {
        *why = "adc_cali_raw_to_voltage failed";
        goto out;
    }
    *vbat_mv = *pin_mv * IDL1_BAT_DIV_NUM / IDL1_BAT_DIV_DEN;
    ok = true;
out:
    xSemaphoreGive(s_lock);
    return ok;
}

int idl1_battery_pct_estimate(int vbat_mv)
{
    /* Typical resting Li-ion curve, mV -> %. */
    static const int mv[]  = { 3300, 3600, 3700, 3750, 3800, 3900, 4000, 4100, 4200 };
    static const int pct[] = {    0,    5,   15,   25,   40,   60,   75,   90,  100 };
    const int n = (int)(sizeof(mv) / sizeof(mv[0]));
    if (vbat_mv <= mv[0]) return 0;
    if (vbat_mv >= mv[n - 1]) return 100;
    for (int i = 1; i < n; i++) {
        if (vbat_mv < mv[i]) {
            return pct[i - 1] + (pct[i] - pct[i - 1]) * (vbat_mv - mv[i - 1]) / (mv[i] - mv[i - 1]);
        }
    }
    return 100;
}
