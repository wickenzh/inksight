#include "st7305.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board_config.h"

#define RLCD_NATIVE_WIDTH 300
#define RLCD_NATIVE_HEIGHT 400
#define RLCD_BYTES_PER_NATIVE_ROW 75
#define RLCD_ROWS_PER_TRANSFER 4
#define RLCD_TRANSFER_BYTES \
    (RLCD_BYTES_PER_NATIVE_ROW * RLCD_ROWS_PER_TRANSFER)

_Static_assert(
    INKSIGHT_WIDTH == 400 && INKSIGHT_HEIGHT == 300,
    "The ST7305 mapping requires a 400x300 landscape framebuffer"
);

static const char *TAG = "st7305";
static spi_device_handle_t s_spi;
static bool s_initialized;
static uint8_t s_transfer_buffer[RLCD_TRANSFER_BYTES];

static esp_err_t transmit_command_data(
    uint8_t command,
    const uint8_t *data,
    size_t data_length
) {
    esp_err_t err = spi_device_acquire_bus(s_spi, portMAX_DELAY);
    if (err != ESP_OK) {
        return err;
    }

    spi_transaction_t command_transaction = {
        .flags = SPI_TRANS_USE_TXDATA |
                 (data_length > 0 ? SPI_TRANS_CS_KEEP_ACTIVE : 0),
        .length = 8,
    };
    command_transaction.tx_data[0] = command;

    gpio_set_level(RLCD_PIN_DC, 0);
    err = spi_device_polling_transmit(s_spi, &command_transaction);

    if (err == ESP_OK && data != NULL && data_length > 0) {
        spi_transaction_t data_transaction = {
            .length = data_length * 8,
            .tx_buffer = data,
        };
        gpio_set_level(RLCD_PIN_DC, 1);
        err = spi_device_polling_transmit(s_spi, &data_transaction);
    }

    spi_device_release_bus(s_spi);
    return err;
}

static esp_err_t transmit_command(uint8_t command) {
    return transmit_command_data(command, NULL, 0);
}

static void hardware_reset(void) {
    gpio_set_level(RLCD_PIN_RESET, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(RLCD_PIN_RESET, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(RLCD_PIN_RESET, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
}

static esp_err_t controller_init(void) {
    const uint8_t d6[] = {0x17, 0x02};
    const uint8_t d1[] = {0x01};
    const uint8_t c0[] = {0x11, 0x04};
    const uint8_t c1[] = {0x69, 0x69, 0x69, 0x69};
    const uint8_t c2[] = {0x19, 0x19, 0x19, 0x19};
    const uint8_t c4[] = {0x4B, 0x4B, 0x4B, 0x4B};
    const uint8_t d8[] = {0x80, 0xE9};
    const uint8_t b2[] = {0x02};
    const uint8_t b3[] = {
        0xE5, 0xF6, 0x05, 0x46, 0x77,
        0x77, 0x77, 0x77, 0x76, 0x45,
    };
    const uint8_t b4[] = {
        0x05, 0x46, 0x77, 0x77, 0x77, 0x77, 0x76, 0x45,
    };
    const uint8_t gate_timing[] = {0x32, 0x03, 0x1F};
    const uint8_t b7[] = {0x13};
    const uint8_t b0[] = {0x64};
    const uint8_t c9[] = {0x00};
    const uint8_t madctl[] = {0x48};
    const uint8_t data_format[] = {0x11};
    const uint8_t b9[] = {0x20};
    const uint8_t b8[] = {0x29};
    const uint8_t full_columns[] = {0x12, 0x2A};
    const uint8_t full_rows[] = {0x00, 0xC7};
    const uint8_t tearing[] = {0x00};
    const uint8_t auto_power_down[] = {0xFF};

    hardware_reset();

#define SEND_DATA(command, value)                                      \
    do {                                                               \
        esp_err_t send_err = transmit_command_data(                    \
            (command),                                                 \
            (value),                                                   \
            sizeof(value)                                              \
        );                                                             \
        if (send_err != ESP_OK) {                                      \
            return send_err;                                           \
        }                                                              \
    } while (0)

    SEND_DATA(0xD6, d6);
    SEND_DATA(0xD1, d1);
    SEND_DATA(0xC0, c0);
    SEND_DATA(0xC1, c1);
    SEND_DATA(0xC2, c2);
    SEND_DATA(0xC4, c4);
    SEND_DATA(0xC5, c2);
    SEND_DATA(0xD8, d8);
    SEND_DATA(0xB2, b2);
    SEND_DATA(0xB3, b3);
    SEND_DATA(0xB4, b4);
    SEND_DATA(0x62, gate_timing);
    SEND_DATA(0xB7, b7);
    SEND_DATA(0xB0, b0);

    ESP_RETURN_ON_ERROR(transmit_command(0x11), TAG, "sleep-out failed");
    vTaskDelay(pdMS_TO_TICKS(120));

    SEND_DATA(0xC9, c9);
    SEND_DATA(0x36, madctl);
    SEND_DATA(0x3A, data_format);
    SEND_DATA(0xB9, b9);
    SEND_DATA(0xB8, b8);
    ESP_RETURN_ON_ERROR(transmit_command(0x21), TAG, "inversion failed");
    SEND_DATA(0x2A, full_columns);
    SEND_DATA(0x2B, full_rows);
    SEND_DATA(0x35, tearing);
    SEND_DATA(0xD0, auto_power_down);
    ESP_RETURN_ON_ERROR(transmit_command(0x38), TAG, "HPM failed");
    ESP_RETURN_ON_ERROR(transmit_command(0x29), TAG, "display-on failed");

#undef SEND_DATA
    return ESP_OK;
}

esp_err_t st7305_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }

    gpio_deep_sleep_hold_dis();
    gpio_hold_dis(RLCD_PIN_SCLK);
    gpio_hold_dis(RLCD_PIN_MOSI);
    gpio_hold_dis(RLCD_PIN_DC);
    gpio_hold_dis(RLCD_PIN_CS);
    gpio_hold_dis(RLCD_PIN_RESET);

    gpio_config_t output_config = {
        .pin_bit_mask = BIT64(RLCD_PIN_DC) |
                        BIT64(RLCD_PIN_RESET),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(
        gpio_config(&output_config),
        TAG,
        "control GPIO configuration failed"
    );

    gpio_config_t te_config = {
        .pin_bit_mask = BIT64(RLCD_PIN_TE),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&te_config), TAG, "TE GPIO failed");

    gpio_set_level(RLCD_PIN_DC, 1);
    gpio_set_level(RLCD_PIN_RESET, 1);

    spi_bus_config_t bus_config = {
        .mosi_io_num = RLCD_PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = RLCD_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = RLCD_TRANSFER_BYTES,
    };
    ESP_RETURN_ON_ERROR(
        spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO),
        TAG,
        "SPI bus initialization failed"
    );

    spi_device_interface_config_t device_config = {
        .clock_speed_hz = INKSIGHT_SPI_CLOCK_HZ,
        .mode = 0,
        .spics_io_num = RLCD_PIN_CS,
        .queue_size = 1,
    };
    ESP_RETURN_ON_ERROR(
        spi_bus_add_device(SPI2_HOST, &device_config, &s_spi),
        TAG,
        "SPI device setup failed"
    );

    ESP_RETURN_ON_ERROR(controller_init(), TAG, "controller init failed");
    s_initialized = true;
    ESP_LOGI(TAG, "ST7305 ready at %d Hz", INKSIGHT_SPI_CLOCK_HZ);
    return ESP_OK;
}

static bool pixel_is_white(const uint8_t *frame, int x, int y) {
    return (frame[y * INKSIGHT_ROW_BYTES + x / 8] &
            (0x80U >> (x % 8))) != 0;
}

static void pack_native_rows(const uint8_t *frame, int native_y_base) {
    memset(s_transfer_buffer, 0, sizeof(s_transfer_buffer));

    // This exactly matches the Waveshare U8g2 R1 mapping:
    // landscape (x, y) -> native (299 - y, x).
    // The ST7305 framebuffer polarity is 1=white and 0=black.
    for (int row_pair = 0; row_pair < RLCD_ROWS_PER_TRANSFER; row_pair++) {
        int logical_x_0 = native_y_base + row_pair * 2;
        int logical_x_1 = logical_x_0 + 1;
        int output_base = row_pair * RLCD_BYTES_PER_NATIVE_ROW;

        for (int native_x = 0; native_x < RLCD_NATIVE_WIDTH; native_x += 4) {
            uint8_t packed = 0;
            for (int lane = 0; lane < 4; lane++) {
                int logical_y =
                    INKSIGHT_HEIGHT - 1 - (native_x + lane);
                if (pixel_is_white(frame, logical_x_0, logical_y)) {
                    packed |= 0x80U >> (lane * 2);
                }
                if (pixel_is_white(frame, logical_x_1, logical_y)) {
                    packed |= 0x40U >> (lane * 2);
                }
            }
            s_transfer_buffer[output_base + native_x / 4] = packed;
        }
    }
}

esp_err_t st7305_display(const uint8_t *landscape_frame) {
    if (landscape_frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(st7305_init(), TAG, "display init failed");
    ESP_RETURN_ON_ERROR(
        transmit_command(0x38),
        TAG,
        "failed to enter high-power mode"
    );

    const uint8_t full_columns[] = {0x12, 0x2A};
    int64_t started_at = esp_timer_get_time();

    for (int native_y = 0;
         native_y < RLCD_NATIVE_HEIGHT;
         native_y += 8) {
        pack_native_rows(landscape_frame, native_y);
        uint8_t row_start = (uint8_t)(native_y / 2);
        uint8_t row_bounds[] = {
            row_start,
            (uint8_t)(row_start + RLCD_ROWS_PER_TRANSFER - 1),
        };

        ESP_RETURN_ON_ERROR(
            transmit_command_data(0x2A, full_columns, sizeof(full_columns)),
            TAG,
            "column window failed"
        );
        ESP_RETURN_ON_ERROR(
            transmit_command_data(0x2B, row_bounds, sizeof(row_bounds)),
            TAG,
            "row window failed"
        );
        ESP_RETURN_ON_ERROR(
            transmit_command_data(
                0x2C,
                s_transfer_buffer,
                sizeof(s_transfer_buffer)
            ),
            TAG,
            "frame transfer failed"
        );
        taskYIELD();
    }

    ESP_LOGI(
        TAG,
        "Frame written in %lld ms",
        (esp_timer_get_time() - started_at) / 1000
    );
    return ESP_OK;
}

esp_err_t st7305_enter_low_power(void) {
    if (!s_initialized) {
        return ESP_OK;
    }
    return transmit_command(0x39);
}

void st7305_prepare_for_deep_sleep(void) {
    if (!s_initialized) {
        return;
    }

    esp_err_t err = st7305_enter_low_power();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Could not enter low-power scan: %s", esp_err_to_name(err));
    }

    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(RLCD_PIN_SCLK, 0);
    gpio_set_level(RLCD_PIN_MOSI, 0);
    gpio_set_level(RLCD_PIN_DC, 1);
    gpio_set_level(RLCD_PIN_CS, 1);
    gpio_set_level(RLCD_PIN_RESET, 1);

    gpio_hold_en(RLCD_PIN_SCLK);
    gpio_hold_en(RLCD_PIN_MOSI);
    gpio_hold_en(RLCD_PIN_DC);
    gpio_hold_en(RLCD_PIN_CS);
    gpio_hold_en(RLCD_PIN_RESET);
    gpio_deep_sleep_hold_en();
}
