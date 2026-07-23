#include "battery.h"

#include <stdbool.h>

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char *TAG = "battery";

float battery_read_voltage(void) {
    adc_oneshot_unit_handle_t adc_handle = NULL;
    adc_cali_handle_t calibration_handle = NULL;

    adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t err = adc_oneshot_new_unit(&unit_config, &adc_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ADC init failed: %s", esp_err_to_name(err));
        return 0.0f;
    }

    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    err = adc_oneshot_config_channel(
        adc_handle,
        ADC_CHANNEL_3,
        &channel_config
    );
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ADC channel config failed: %s", esp_err_to_name(err));
        adc_oneshot_del_unit(adc_handle);
        return 0.0f;
    }

    adc_cali_curve_fitting_config_t calibration_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    bool calibrated =
        adc_cali_create_scheme_curve_fitting(
            &calibration_config,
            &calibration_handle
        ) == ESP_OK;

    int readings[16] = {0};
    int valid_readings = 0;
    for (size_t index = 0; index < 16; index++) {
        if (adc_oneshot_read(
                adc_handle,
                ADC_CHANNEL_3,
                &readings[valid_readings]
            ) == ESP_OK) {
            valid_readings++;
        }
    }

    for (int i = 0; i < valid_readings - 1; i++) {
        for (int j = i + 1; j < valid_readings; j++) {
            if (readings[j] < readings[i]) {
                int temporary = readings[i];
                readings[i] = readings[j];
                readings[j] = temporary;
            }
        }
    }

    int first = valid_readings > 4 ? 2 : 0;
    int last = valid_readings > 4 ? valid_readings - 2 : valid_readings;
    int64_t raw_sum = 0;
    for (int index = first; index < last; index++) {
        raw_sum += readings[index];
    }
    int raw_average = last > first ? (int)(raw_sum / (last - first)) : 0;

    int adc_millivolts = 0;
    if (calibrated) {
        adc_cali_raw_to_voltage(
            calibration_handle,
            raw_average,
            &adc_millivolts
        );
    } else {
        adc_millivolts = raw_average * 3300 / 4095;
    }

    if (calibration_handle != NULL) {
        adc_cali_delete_scheme_curve_fitting(calibration_handle);
    }
    adc_oneshot_del_unit(adc_handle);

    // Board schematic: VBAT -> 200 K -> GPIO4 -> 100 K -> GND.
    float battery_voltage = (adc_millivolts / 1000.0f) * 3.0f;
    ESP_LOGI(
        TAG,
        "raw=%d adc=%dmV battery=%.2fV",
        raw_average,
        adc_millivolts,
        battery_voltage
    );
    return battery_voltage;
}
