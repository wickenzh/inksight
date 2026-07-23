#pragma once

#include "esp_err.h"

#include "config_store.h"

esp_err_t portal_start(inksight_config_t *config, const char *ap_name);
