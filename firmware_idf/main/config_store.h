#pragma once

#include <stdbool.h>

#include "esp_err.h"

typedef struct {
    char ssid[33];
    char password[65];
    char server[201];
    char device_token[193];
    int sleep_minutes;
} inksight_config_t;

void config_store_defaults(inksight_config_t *config);
esp_err_t config_store_load(inksight_config_t *config);
esp_err_t config_store_save(const inksight_config_t *config);
esp_err_t config_store_save_device_token(const char *token);
esp_err_t config_store_clear_device_token(void);
esp_err_t config_store_save_sleep_minutes(int sleep_minutes);
bool config_store_is_ready(const inksight_config_t *config);
