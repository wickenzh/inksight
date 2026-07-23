#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#include "config_store.h"

esp_err_t wifi_manager_init(void);
bool wifi_manager_connect(const inksight_config_t *config);
esp_err_t wifi_manager_start_provisioning_ap(char *ap_name, size_t ap_name_size);
bool wifi_manager_is_connected(void);
int wifi_manager_rssi(void);
void wifi_manager_mac_string(char *buffer, size_t buffer_size);
int wifi_manager_scan_json(char *buffer, size_t buffer_size);
void wifi_manager_stop(void);
