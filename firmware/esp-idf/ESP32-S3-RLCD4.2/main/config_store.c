#include "config_store.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"

#include "board_config.h"

static const char *TAG = "config_store";

static void read_string(
    nvs_handle_t handle,
    const char *key,
    char *destination,
    size_t destination_size,
    const char *fallback
) {
    size_t required = destination_size;
    esp_err_t err = nvs_get_str(handle, key, destination, &required);
    if (err != ESP_OK) {
        strlcpy(
            destination,
            fallback != NULL ? fallback : "",
            destination_size
        );
    }
}

void config_store_defaults(inksight_config_t *config) {
    if (config == NULL) {
        return;
    }
    memset(config, 0, sizeof(*config));
    strlcpy(
        config->server,
        INKSIGHT_DEFAULT_SERVER_URL,
        sizeof(config->server)
    );
    config->sleep_minutes = INKSIGHT_DEFAULT_SLEEP_MINUTES;
}

esp_err_t config_store_load(inksight_config_t *config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    config_store_defaults(config);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(INKSIGHT_CONFIG_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    int32_t version = 0;
    err = nvs_get_i32(handle, "cfg_version", &version);
    if (err != ESP_OK || version != INKSIGHT_CONFIG_VERSION) {
        ESP_LOGI(TAG, "No compatible configuration in NVS");
        nvs_close(handle);
        return ESP_OK;
    }

    read_string(handle, "ssid", config->ssid, sizeof(config->ssid), "");
    read_string(
        handle,
        "pass",
        config->password,
        sizeof(config->password),
        ""
    );
    read_string(
        handle,
        "server",
        config->server,
        sizeof(config->server),
        INKSIGHT_DEFAULT_SERVER_URL
    );
    if (config->server[0] == '\0') {
        strlcpy(
            config->server,
            INKSIGHT_DEFAULT_SERVER_URL,
            sizeof(config->server)
        );
    }
    read_string(
        handle,
        "device_token",
        config->device_token,
        sizeof(config->device_token),
        ""
    );

    int32_t sleep_minutes = INKSIGHT_DEFAULT_SLEEP_MINUTES;
    if (nvs_get_i32(handle, "sleep_min", &sleep_minutes) == ESP_OK &&
        sleep_minutes >= INKSIGHT_MIN_SLEEP_MINUTES &&
        sleep_minutes <= INKSIGHT_MAX_SLEEP_MINUTES) {
        config->sleep_minutes = sleep_minutes;
    }

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t config_store_save(const inksight_config_t *config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(INKSIGHT_CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_i32(handle, "cfg_version", INKSIGHT_CONFIG_VERSION);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, "ssid", config->ssid);
    }
    if (err == ESP_OK) {
        err = nvs_set_str(handle, "pass", config->password);
    }
    if (err == ESP_OK) {
        err = nvs_set_str(handle, "server", config->server);
    }
    if (err == ESP_OK) {
        err = nvs_set_i32(handle, "sleep_min", config->sleep_minutes);
    }
    if (err == ESP_OK) {
        if (config->device_token[0] != '\0') {
            err = nvs_set_str(handle, "device_token", config->device_token);
        } else {
            err = nvs_erase_key(handle, "device_token");
            if (err == ESP_ERR_NVS_NOT_FOUND) {
                err = ESP_OK;
            }
        }
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}

esp_err_t config_store_save_device_token(const char *token) {
    if (token == NULL || token[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(INKSIGHT_CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_str(handle, "device_token", token);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

esp_err_t config_store_clear_device_token(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(INKSIGHT_CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_erase_key(handle, "device_token");
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = ESP_OK;
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

esp_err_t config_store_save_sleep_minutes(int sleep_minutes) {
    if (sleep_minutes < INKSIGHT_MIN_SLEEP_MINUTES ||
        sleep_minutes > INKSIGHT_MAX_SLEEP_MINUTES) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(INKSIGHT_CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_i32(handle, "sleep_min", sleep_minutes);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

bool config_store_is_ready(const inksight_config_t *config) {
    if (config == NULL || config->ssid[0] == '\0') {
        return false;
    }
    return strncmp(config->server, "http://", 7) == 0 ||
           strncmp(config->server, "https://", 8) == 0;
}
