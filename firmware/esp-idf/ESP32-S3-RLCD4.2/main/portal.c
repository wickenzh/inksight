#include "portal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board_config.h"
#include "wifi_manager.h"

static const char *TAG = "portal";
static inksight_config_t *s_config;
static char s_ap_name[33];

static const char *PAGE_TEMPLATE =
    "<!doctype html><html lang='zh-CN'><head>"
    "<meta charset='utf-8'><meta name='viewport' "
    "content='width=device-width,initial-scale=1'>"
    "<title>InkSight Setup</title><style>"
    "body{margin:0;background:#f4f1e8;color:#17211b;font:16px system-ui}"
    "main{max-width:520px;margin:32px auto;padding:24px}"
    ".card{background:#fff;border:1px solid #b9c2b5;border-radius:18px;"
    "padding:24px;box-shadow:0 12px 35px #23332618}"
    "h1{margin:0 0 6px;font-size:28px}p{color:#506056}"
    "label{display:block;margin:18px 0 6px;font-weight:650}"
    "input{box-sizing:border-box;width:100%%;padding:12px;border:1px solid "
    "#9ca99f;border-radius:9px;font:inherit;background:#fcfdf9}"
    "button{width:100%%;margin-top:24px;padding:13px;border:0;border-radius:9px;"
    "background:#173f2e;color:#fff;font:inherit;font-weight:700}"
    "small{display:block;margin-top:12px;color:#6b786f}</style></head><body>"
    "<main><div class='card'><h1>InkSight</h1>"
    "<p>Waveshare ESP32-S3-RLCD-4.2 · ESP-IDF</p>"
    "<form method='post' action='/save'>"
    "<label>Wi-Fi 名称</label><input name='ssid' list='networks' "
    "maxlength='32' required value='%s'><datalist id='networks'></datalist>"
    "<label>Wi-Fi 密码</label><input name='password' type='password' "
    "maxlength='64' value=''>"
    "<label>InkSight 服务地址</label><input name='server' type='url' "
    "maxlength='200' required placeholder='https://example.com' value='%s'>"
    "<label>刷新间隔（分钟）</label><input name='sleep' type='number' "
    "min='10' max='1440' value='%d'>"
    "<button type='submit'>保存并重启</button></form>"
    "<small>配网热点：%s · 设备地址：192.168.4.1</small></div></main>"
    "<script>fetch('/scan').then(r=>r.json()).then(d=>{"
    "const l=document.querySelector('#networks');"
    "d.networks.forEach(n=>{const o=document.createElement('option');"
    "o.value=n.ssid;l.appendChild(o)})}).catch(()=>{})</script>"
    "</body></html>";

static void html_escape(
    const char *source,
    char *destination,
    size_t destination_size
) {
    if (destination_size == 0) {
        return;
    }
    size_t output = 0;
    for (size_t input = 0;
         source != NULL && source[input] != '\0' &&
         output + 1 < destination_size;
         input++) {
        const char *replacement = NULL;
        switch (source[input]) {
            case '&': replacement = "&amp;"; break;
            case '<': replacement = "&lt;"; break;
            case '>': replacement = "&gt;"; break;
            case '"': replacement = "&quot;"; break;
            case '\'': replacement = "&#39;"; break;
            default:
                destination[output++] = source[input];
                continue;
        }
        size_t replacement_length = strlen(replacement);
        if (output + replacement_length >= destination_size) {
            break;
        }
        memcpy(destination + output, replacement, replacement_length);
        output += replacement_length;
    }
    destination[output] = '\0';
}

static esp_err_t root_handler(httpd_req_t *request) {
    char escaped_ssid[160];
    char escaped_server[1024];
    html_escape(s_config->ssid, escaped_ssid, sizeof(escaped_ssid));
    html_escape(s_config->server, escaped_server, sizeof(escaped_server));

    size_t page_capacity =
        strlen(PAGE_TEMPLATE) + strlen(escaped_ssid) +
        strlen(escaped_server) + strlen(s_ap_name) + 64;
    char *page = malloc(page_capacity);
    if (page == NULL) {
        return httpd_resp_send_500(request);
    }

    snprintf(
        page,
        page_capacity,
        PAGE_TEMPLATE,
        escaped_ssid,
        escaped_server,
        s_config->sleep_minutes,
        s_ap_name
    );
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    esp_err_t err = httpd_resp_send(request, page, HTTPD_RESP_USE_STRLEN);
    free(page);
    return err;
}

static esp_err_t scan_handler(httpd_req_t *request) {
    char *json = malloc(6144);
    if (json == NULL) {
        return httpd_resp_send_500(request);
    }
    int length = wifi_manager_scan_json(json, 6144);
    if (length < 0) {
        free(json);
        return httpd_resp_send_err(
            request,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Wi-Fi scan failed"
        );
    }

    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_send(request, json, length);
    free(json);
    return err;
}

static int hex_value(char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

static void url_decode(char *value) {
    char *read_pointer = value;
    char *write_pointer = value;
    while (*read_pointer != '\0') {
        if (*read_pointer == '+') {
            *write_pointer++ = ' ';
            read_pointer++;
            continue;
        }
        if (*read_pointer == '%' &&
            read_pointer[1] != '\0' &&
            read_pointer[2] != '\0') {
            int high = hex_value(read_pointer[1]);
            int low = hex_value(read_pointer[2]);
            if (high >= 0 && low >= 0) {
                *write_pointer++ = (char)((high << 4) | low);
                read_pointer += 3;
                continue;
            }
        }
        *write_pointer++ = *read_pointer++;
    }
    *write_pointer = '\0';
}

static bool form_value(
    const char *body,
    const char *key,
    char *destination,
    size_t destination_size
) {
    size_t key_length = strlen(key);
    const char *cursor = body;
    while (cursor != NULL && *cursor != '\0') {
        const char *pair_end = strchr(cursor, '&');
        size_t pair_length =
            pair_end != NULL ? (size_t)(pair_end - cursor) : strlen(cursor);
        if (pair_length > key_length &&
            strncmp(cursor, key, key_length) == 0 &&
            cursor[key_length] == '=') {
            const char *value = cursor + key_length + 1;
            size_t value_length =
                pair_length - key_length - 1;
            if (value_length >= destination_size) {
                value_length = destination_size - 1;
            }
            memcpy(destination, value, value_length);
            destination[value_length] = '\0';
            url_decode(destination);
            return true;
        }
        cursor = pair_end != NULL ? pair_end + 1 : NULL;
    }
    destination[0] = '\0';
    return false;
}

static void trim(char *value) {
    char *start = value;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        start++;
    }
    if (start != value) {
        memmove(value, start, strlen(start) + 1);
    }

    size_t length = strlen(value);
    while (length > 0 && isspace((unsigned char)value[length - 1])) {
        value[--length] = '\0';
    }
}

static void restart_task(void *argument) {
    (void)argument;
    vTaskDelay(pdMS_TO_TICKS(1200));
    esp_restart();
}

static esp_err_t save_handler(httpd_req_t *request) {
    if (request->content_len <= 0 || request->content_len > 1024) {
        return httpd_resp_send_err(
            request,
            HTTPD_400_BAD_REQUEST,
            "Invalid form body"
        );
    }

    char *body = calloc((size_t)request->content_len + 1, 1);
    if (body == NULL) {
        return httpd_resp_send_500(request);
    }

    int received = 0;
    while (received < request->content_len) {
        int result = httpd_req_recv(
            request,
            body + received,
            request->content_len - received
        );
        if (result <= 0) {
            free(body);
            return ESP_FAIL;
        }
        received += result;
    }

    inksight_config_t next = *s_config;
    char sleep_value[12];
    char submitted_password[65];
    bool has_ssid =
        form_value(body, "ssid", next.ssid, sizeof(next.ssid));
    form_value(
        body,
        "password",
        submitted_password,
        sizeof(submitted_password)
    );
    bool has_server =
        form_value(body, "server", next.server, sizeof(next.server));
    bool has_sleep =
        form_value(body, "sleep", sleep_value, sizeof(sleep_value));
    free(body);

    trim(next.ssid);
    trim(next.server);
    if (submitted_password[0] != '\0' ||
        strcmp(next.ssid, s_config->ssid) != 0) {
        strlcpy(
            next.password,
            submitted_password,
            sizeof(next.password)
        );
    }
    while (strlen(next.server) > 0 &&
           next.server[strlen(next.server) - 1] == '/') {
        next.server[strlen(next.server) - 1] = '\0';
    }

    char *sleep_end = NULL;
    long sleep_minutes = strtol(sleep_value, &sleep_end, 10);
    bool valid_server =
        strncmp(next.server, "http://", 7) == 0 ||
        strncmp(next.server, "https://", 8) == 0;
    if (!has_ssid || next.ssid[0] == '\0' ||
        !has_server || !valid_server ||
        !has_sleep || sleep_end == sleep_value ||
        *sleep_end != '\0' ||
        sleep_minutes < INKSIGHT_MIN_SLEEP_MINUTES ||
        sleep_minutes > INKSIGHT_MAX_SLEEP_MINUTES) {
        return httpd_resp_send_err(
            request,
            HTTPD_400_BAD_REQUEST,
            "Check SSID, server URL, and refresh interval"
        );
    }

    if (strcmp(next.server, s_config->server) != 0) {
        next.device_token[0] = '\0';
    }
    next.sleep_minutes = (int)sleep_minutes;

    esp_err_t err = config_store_save(&next);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not save configuration: %s", esp_err_to_name(err));
        return httpd_resp_send_500(request);
    }
    *s_config = next;

    static const char response[] =
        "<!doctype html><meta charset='utf-8'><meta name='viewport' "
        "content='width=device-width'><title>Saved</title>"
        "<body style='font:18px system-ui;padding:40px'>"
        "<h1>已保存</h1><p>设备正在重启并连接 InkSight。</p></body>";
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    err = httpd_resp_send(request, response, HTTPD_RESP_USE_STRLEN);
    xTaskCreate(restart_task, "portal_restart", 2048, NULL, 5, NULL);
    return err;
}

esp_err_t portal_start(inksight_config_t *config, const char *ap_name) {
    if (config == NULL || ap_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_config = config;
    strlcpy(s_ap_name, ap_name, sizeof(s_ap_name));

    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    server_config.max_uri_handlers = 8;
    server_config.stack_size = 6144;

    httpd_handle_t server = NULL;
    ESP_RETURN_ON_ERROR(
        httpd_start(&server, &server_config),
        TAG,
        "HTTP server start failed"
    );

    const httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
    };
    const httpd_uri_t scan_uri = {
        .uri = "/scan",
        .method = HTTP_GET,
        .handler = scan_handler,
    };
    const httpd_uri_t save_uri = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = save_handler,
    };
    const httpd_uri_t generate_204_uri = {
        .uri = "/generate_204",
        .method = HTTP_GET,
        .handler = root_handler,
    };
    const httpd_uri_t hotspot_uri = {
        .uri = "/hotspot-detect.html",
        .method = HTTP_GET,
        .handler = root_handler,
    };

    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &root_uri),
        TAG,
        "root handler failed"
    );
    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &scan_uri),
        TAG,
        "scan handler failed"
    );
    ESP_RETURN_ON_ERROR(
        httpd_register_uri_handler(server, &save_uri),
        TAG,
        "save handler failed"
    );
    httpd_register_uri_handler(server, &generate_204_uri);
    httpd_register_uri_handler(server, &hotspot_uri);

    ESP_LOGI(TAG, "Provisioning page ready at http://192.168.4.1");
    return ESP_OK;
}
