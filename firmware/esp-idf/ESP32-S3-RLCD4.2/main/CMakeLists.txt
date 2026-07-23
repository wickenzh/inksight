idf_component_register(
    SRCS
        "app_main.c"
        "backend.c"
        "battery.c"
        "config_store.c"
        "portal.c"
        "st7305.c"
        "ui.c"
        "wifi_manager.c"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        esp_adc
        esp_event
        esp_http_client
        esp_http_server
        esp_netif
        esp_timer
        esp_wifi
        json
        mbedtls
        nvs_flash
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
