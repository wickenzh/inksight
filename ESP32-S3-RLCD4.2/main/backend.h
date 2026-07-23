#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#include "config_store.h"

typedef struct {
    bool is_fallback;
    int refresh_minutes;
    char mode_id[65];
} backend_render_info_t;

esp_err_t backend_render(
    inksight_config_t *config,
    bool next_mode,
    float battery_voltage,
    int wifi_rssi,
    uint8_t *frame,
    backend_render_info_t *info
);

esp_err_t backend_decode_bmp(
    const uint8_t *data,
    size_t length,
    uint8_t *frame
);
