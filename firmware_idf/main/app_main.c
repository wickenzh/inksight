#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "backend.h"
#include "battery.h"
#include "board_config.h"
#include "config_store.h"
#include "portal.h"
#include "st7305.h"
#include "ui.h"
#include "wifi_manager.h"

static const char *TAG = "inksight";
static uint8_t s_frame[INKSIGHT_FRAME_BYTES];
static inksight_config_t s_config;

static void initialize_nvs(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

static void initialize_key(void) {
    gpio_config_t key_config = {
        .pin_bit_mask = 1ULL << INKSIGHT_KEY_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&key_config));
}

static bool key_held_for(uint32_t duration_ms) {
    if (gpio_get_level(INKSIGHT_KEY_GPIO) != 0) {
        return false;
    }
    uint32_t elapsed = 0;
    while (elapsed < duration_ms) {
        if (gpio_get_level(INKSIGHT_KEY_GPIO) != 0) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
        elapsed += 20;
    }
    return true;
}

static void run_provisioning(void) {
    char ap_name[33];
    ESP_ERROR_CHECK(
        wifi_manager_start_provisioning_ap(ap_name, sizeof(ap_name))
    );
    ui_draw_setup(s_frame, ap_name);
    ESP_ERROR_CHECK(st7305_display(s_frame));
    ESP_ERROR_CHECK(portal_start(&s_config, ap_name));

    ESP_LOGI(TAG, "Provisioning active; connect to %s", ap_name);
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void wait_for_key_release(void) {
    uint32_t waited_ms = 0;
    while (gpio_get_level(INKSIGHT_KEY_GPIO) == 0 && waited_ms < 5000) {
        vTaskDelay(pdMS_TO_TICKS(20));
        waited_ms += 20;
    }
}

static void enter_deep_sleep(int sleep_minutes) {
    if (sleep_minutes < INKSIGHT_MIN_SLEEP_MINUTES ||
        sleep_minutes > INKSIGHT_MAX_SLEEP_MINUTES) {
        sleep_minutes = INKSIGHT_DEFAULT_SLEEP_MINUTES;
    }

    wifi_manager_stop();
    wait_for_key_release();

    ESP_ERROR_CHECK(
        esp_sleep_enable_timer_wakeup(
            (uint64_t)sleep_minutes * 60ULL * 1000000ULL
        )
    );
    ESP_ERROR_CHECK(
        esp_sleep_enable_ext1_wakeup_io(
            1ULL << INKSIGHT_KEY_GPIO,
            ESP_EXT1_WAKEUP_ANY_LOW
        )
    );
    gpio_pullup_en(INKSIGHT_KEY_GPIO);
    gpio_pulldown_dis(INKSIGHT_KEY_GPIO);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

    st7305_prepare_for_deep_sleep();
    ESP_LOGI(
        TAG,
        "Deep sleep for %d minutes; RLCD low-power scan remains active",
        sleep_minutes
    );
    vTaskDelay(pdMS_TO_TICKS(50));
    esp_deep_sleep_start();
}

void app_main(void) {
    ESP_LOGI(
        TAG,
        "InkSight ESP-IDF %s for Waveshare ESP32-S3-RLCD-4.2",
        INKSIGHT_IDF_VERSION
    );
    ESP_LOGI(
        TAG,
        "Free heap=%u, PSRAM=%u",
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM)
    );

    initialize_nvs();
    initialize_key();
    ESP_ERROR_CHECK(config_store_load(&s_config));
    ESP_ERROR_CHECK(st7305_init());
    ESP_ERROR_CHECK(wifi_manager_init());

    esp_sleep_wakeup_cause_t wake_cause = esp_sleep_get_wakeup_cause();
    bool key_wakeup = wake_cause == ESP_SLEEP_WAKEUP_EXT1;
    bool force_provisioning =
        key_held_for(key_wakeup ? 1500 : 500);
    bool next_mode = key_wakeup && !force_provisioning;

    if (force_provisioning || !config_store_is_ready(&s_config)) {
        run_provisioning();
    }

    if (!wifi_manager_connect(&s_config)) {
        ESP_LOGW(TAG, "Saved Wi-Fi unavailable; opening provisioning");
        run_provisioning();
    }

    float battery_voltage = battery_read_voltage();
    ESP_LOGI(
        TAG,
        "Main stack free before backend=%u bytes",
        (unsigned)uxTaskGetStackHighWaterMark(NULL)
    );
    backend_render_info_t render_info;
    esp_err_t render_error = backend_render(
        &s_config,
        next_mode,
        battery_voltage,
        wifi_manager_rssi(),
        s_frame,
        &render_info
    );
    ESP_LOGI(
        TAG,
        "Main stack minimum free after backend=%u bytes",
        (unsigned)uxTaskGetStackHighWaterMark(NULL)
    );

    if (render_error == ESP_OK) {
        ESP_LOGI(
            TAG,
            "Rendered mode=%s fallback=%s refresh=%d",
            render_info.mode_id[0] != '\0' ? render_info.mode_id : "(unknown)",
            render_info.is_fallback ? "yes" : "no",
            render_info.refresh_minutes
        );
        ESP_ERROR_CHECK(st7305_display(s_frame));
    } else {
        ESP_LOGE(
            TAG,
            "Render failed: %s",
            esp_err_to_name(render_error)
        );
        char error_line[32];
        snprintf(
            error_line,
            sizeof(error_line),
            "ERROR %s",
            esp_err_to_name(render_error)
        );
        ui_draw_status(
            s_frame,
            "FETCH FAILED",
            "CHECK WIFI SERVER",
            error_line,
            "HOLD KEY FOR SETUP"
        );
        ESP_ERROR_CHECK(st7305_display(s_frame));
    }

    enter_deep_sleep(s_config.sleep_minutes);
}
