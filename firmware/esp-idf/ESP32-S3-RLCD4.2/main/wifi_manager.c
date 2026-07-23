#include "wifi_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "board_config.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAILED_BIT BIT1
#define WIFI_MAX_RETRIES 3

static const char *TAG = "wifi_manager";
static EventGroupHandle_t s_event_group;
static bool s_initialized;
static bool s_started;
static bool s_connecting;
static int s_retry_count;

static void wifi_event_handler(
    void *handler_argument,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
) {
    (void)handler_argument;
    (void)event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (s_connecting) {
            esp_wifi_connect();
        }
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_event_group, WIFI_CONNECTED_BIT);
        if (s_connecting && s_retry_count < WIFI_MAX_RETRIES) {
            s_retry_count++;
            ESP_LOGI(
                TAG,
                "Retrying station connection (%d/%d)",
                s_retry_count,
                WIFI_MAX_RETRIES
            );
            esp_wifi_connect();
        } else if (s_connecting) {
            s_connecting = false;
            xEventGroupSetBits(s_event_group, WIFI_FAILED_BIT);
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = event_data;
        ESP_LOGI(TAG, "Station IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_count = 0;
        s_connecting = false;
        xEventGroupSetBits(s_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_manager_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    if (esp_netif_create_default_wifi_sta() == NULL ||
        esp_netif_create_default_wifi_ap() == NULL) {
        return ESP_FAIL;
    }

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(
        esp_wifi_init(&init_config),
        TAG,
        "Wi-Fi driver initialization failed"
    );

    s_event_group = xEventGroupCreate();
    if (s_event_group == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            wifi_event_handler,
            NULL,
            NULL
        ),
        TAG,
        "Wi-Fi event registration failed"
    );
    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            wifi_event_handler,
            NULL,
            NULL
        ),
        TAG,
        "IP event registration failed"
    );

    ESP_RETURN_ON_ERROR(
        esp_wifi_set_storage(WIFI_STORAGE_RAM),
        TAG,
        "Wi-Fi RAM storage setup failed"
    );

    s_initialized = true;
    return ESP_OK;
}

bool wifi_manager_connect(const inksight_config_t *config) {
    if (config == NULL || config->ssid[0] == '\0') {
        return false;
    }
    if (wifi_manager_init() != ESP_OK) {
        return false;
    }

    wifi_config_t station_config = {0};
    strlcpy(
        (char *)station_config.sta.ssid,
        config->ssid,
        sizeof(station_config.sta.ssid)
    );
    strlcpy(
        (char *)station_config.sta.password,
        config->password,
        sizeof(station_config.sta.password)
    );
    station_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    station_config.sta.pmf_cfg.capable = true;
    station_config.sta.pmf_cfg.required = false;

    xEventGroupClearBits(
        s_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAILED_BIT
    );
    s_retry_count = 0;
    s_connecting = true;

    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK ||
        esp_wifi_set_config(WIFI_IF_STA, &station_config) != ESP_OK) {
        s_connecting = false;
        return false;
    }

    if (!s_started) {
        if (esp_wifi_start() != ESP_OK) {
            s_connecting = false;
            return false;
        }
        s_started = true;
    } else {
        esp_wifi_disconnect();
        if (esp_wifi_connect() != ESP_OK) {
            s_connecting = false;
            return false;
        }
    }

    EventBits_t bits = xEventGroupWaitBits(
        s_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAILED_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(INKSIGHT_WIFI_TIMEOUT_MS)
    );
    bool connected = (bits & WIFI_CONNECTED_BIT) != 0;
    if (!connected) {
        ESP_LOGW(TAG, "Could not connect to SSID %s", config->ssid);
        s_connecting = false;
    }
    return connected;
}

esp_err_t wifi_manager_start_provisioning_ap(
    char *ap_name,
    size_t ap_name_size
) {
    if (ap_name == NULL || ap_name_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(
        wifi_manager_init(),
        TAG,
        "Wi-Fi initialization failed"
    );

    uint8_t mac[6] = {0};
    ESP_RETURN_ON_ERROR(
        esp_read_mac(mac, ESP_MAC_WIFI_STA),
        TAG,
        "MAC read failed"
    );
    snprintf(
        ap_name,
        ap_name_size,
        "InkSight-%02X%02X%02X",
        mac[3],
        mac[4],
        mac[5]
    );

    wifi_config_t access_point_config = {0};
    strlcpy(
        (char *)access_point_config.ap.ssid,
        ap_name,
        sizeof(access_point_config.ap.ssid)
    );
    access_point_config.ap.ssid_len = strlen(ap_name);
    access_point_config.ap.channel = 1;
    access_point_config.ap.max_connection = 4;
    access_point_config.ap.authmode = WIFI_AUTH_OPEN;
    access_point_config.ap.pmf_cfg.required = false;

    s_connecting = false;
    ESP_RETURN_ON_ERROR(
        esp_wifi_set_mode(WIFI_MODE_APSTA),
        TAG,
        "AP+STA mode failed"
    );
    ESP_RETURN_ON_ERROR(
        esp_wifi_set_config(WIFI_IF_AP, &access_point_config),
        TAG,
        "AP configuration failed"
    );

    if (!s_started) {
        ESP_RETURN_ON_ERROR(
            esp_wifi_start(),
            TAG,
            "AP start failed"
        );
        s_started = true;
    }

    ESP_LOGI(TAG, "Provisioning AP: %s (192.168.4.1)", ap_name);
    return ESP_OK;
}

bool wifi_manager_is_connected(void) {
    if (!s_initialized || s_event_group == NULL) {
        return false;
    }
    return (xEventGroupGetBits(s_event_group) & WIFI_CONNECTED_BIT) != 0;
}

int wifi_manager_rssi(void) {
    wifi_ap_record_t access_point = {0};
    if (esp_wifi_sta_get_ap_info(&access_point) != ESP_OK) {
        return 0;
    }
    return access_point.rssi;
}

void wifi_manager_mac_string(char *buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) {
        return;
    }

    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
        buffer[0] = '\0';
        return;
    }
    snprintf(
        buffer,
        buffer_size,
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]
    );
}

static int compare_access_points(const void *left, const void *right) {
    const wifi_ap_record_t *left_record = left;
    const wifi_ap_record_t *right_record = right;
    return right_record->rssi - left_record->rssi;
}

int wifi_manager_scan_json(char *buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0 || !s_started) {
        return -1;
    }

    wifi_scan_config_t scan_config = {
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };
    if (esp_wifi_scan_start(&scan_config, true) != ESP_OK) {
        return -1;
    }

    uint16_t access_point_count = 0;
    if (esp_wifi_scan_get_ap_num(&access_point_count) != ESP_OK) {
        return -1;
    }
    if (access_point_count > 32) {
        access_point_count = 32;
    }

    wifi_ap_record_t *records =
        calloc(access_point_count, sizeof(wifi_ap_record_t));
    if (records == NULL && access_point_count > 0) {
        return -1;
    }
    if (access_point_count > 0 &&
        esp_wifi_scan_get_ap_records(&access_point_count, records) != ESP_OK) {
        free(records);
        return -1;
    }
    qsort(
        records,
        access_point_count,
        sizeof(wifi_ap_record_t),
        compare_access_points
    );

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        free(records);
        return -1;
    }
    cJSON *networks = cJSON_AddArrayToObject(root, "networks");
    if (networks == NULL) {
        cJSON_Delete(root);
        free(records);
        return -1;
    }

    int added = 0;
    for (uint16_t index = 0;
         index < access_point_count && added < 20;
         index++) {
        if (records[index].ssid[0] == '\0') {
            continue;
        }
        bool duplicate = false;
        for (uint16_t previous = 0; previous < index; previous++) {
            if (strcmp(
                    (const char *)records[index].ssid,
                    (const char *)records[previous].ssid
                ) == 0) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }

        cJSON *network = cJSON_CreateObject();
        if (network == NULL) {
            continue;
        }
        cJSON_AddStringToObject(
            network,
            "ssid",
            (const char *)records[index].ssid
        );
        cJSON_AddNumberToObject(network, "rssi", records[index].rssi);
        cJSON_AddBoolToObject(
            network,
            "secure",
            records[index].authmode != WIFI_AUTH_OPEN
        );
        cJSON_AddItemToArray(networks, network);
        added++;
    }

    bool printed = cJSON_PrintPreallocated(
        root,
        buffer,
        (int)buffer_size,
        false
    );
    cJSON_Delete(root);
    free(records);
    return printed ? (int)strlen(buffer) : -1;
}

void wifi_manager_stop(void) {
    if (!s_started) {
        return;
    }
    s_connecting = false;
    esp_wifi_disconnect();
    esp_wifi_stop();
    s_started = false;
    xEventGroupClearBits(
        s_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAILED_BIT
    );
}
