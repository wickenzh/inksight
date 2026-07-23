#pragma once

#include <stdint.h>

#include "esp_err.h"

esp_err_t st7305_init(void);
esp_err_t st7305_display(const uint8_t *landscape_frame);
esp_err_t st7305_enter_low_power(void);
void st7305_prepare_for_deep_sleep(void);
