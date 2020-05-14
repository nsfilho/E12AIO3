/**
 * E12AIO3 Firmware
 * Copyright (C) 2020 E01-AIO Automação Ltda.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * Author: Nelio Santos <nsfilho@icloud.com>
 * Repository: https://github.com/nsfilho/E12AIO3
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <fcntl.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_log.h>
#include <esp_spiffs.h>
#include <mbedtls/base64.h>
#include <cJSON.h>
#include "e12aio.h"
#include "config.h"
#include "wifi.h"
#include "mqtt.h"
#include "httpd.h"
#include "utils.h"

#ifdef CONFIG_COMPONENT_HTTPD
static const char *TAG = "httpd.cpp";
static httpd_handle_t g_server = NULL;

static char **g_userctx = NULL;
static uint8_t g_userctx_len = 0;

void e12aio_httpd_init_task(void *arg)
{
    e12aio_config_wait_load(TAG);
    e12aio_httpd_start();
    vTaskDelete(NULL);
}
#endif

void e12aio_httpd_init()
{
#ifdef CONFIG_COMPONENT_HTTPD
    xTaskCreate(e12aio_httpd_init_task, "http_init", 2048, NULL, 5, NULL);
#endif
}

#ifdef CONFIG_COMPONENT_HTTPD

/**
 * @brief Check if the basic auth informations are correctly (user and pass)
 * @param req http request
 * @return 
 *      true if its valid
 *      false to incorrect user/pass.
 */
bool e12aio_httpd_auth_valid(httpd_req_t *req)
{
    size_t l_hdr_len = httpd_req_get_hdr_value_len(req, "Authorization");
    if (l_hdr_len == 0)
        return false;

    // TODO: validate authentication
    char l_hdr_buff[l_hdr_len + 1];
    if (httpd_req_get_hdr_value_str(req, "Authorization", l_hdr_buff, l_hdr_len + 1) == ESP_OK)
    {
        char *l_partType = strtok(l_hdr_buff, " ");
        char *l_partAuth64 = strtok(NULL, " ");
        char l_decoded[50];
        size_t l_decodedSize;
        ESP_LOGD(TAG, "Auth Type: %s - [%s]", l_partType, l_partAuth64);
        if (mbedtls_base64_decode((unsigned char *)&l_decoded, 50, &l_decodedSize, (unsigned char *)l_partAuth64, strlen(l_partAuth64)) == 0)
        {
            l_decoded[l_decodedSize] = 0;
            char *l_auth_username = strtok(l_decoded, ":");
            char *l_auth_password = strtok(NULL, ":");
            ESP_LOGD(TAG, "Auth Decoded - User: [%s] - Password: [%s]", l_auth_username, l_auth_password);
            const char *l_config_username = e12aio_config_get()->httpd.username;
            const char *l_config_password = e12aio_config_get()->httpd.password;
            ESP_LOGD(TAG, "Auth Config - User: [%s] - Password: [%s]", l_config_username, l_config_password);
            if (strncasecmp(l_auth_username, l_config_username, strlen(l_config_username)) == 0 &&
                strncasecmp(l_auth_password, l_config_password, strlen(l_config_password)) == 0)
                return true;
        }
    }
    return false;
}

/**
 * @brief This function handle all basic authentication needs. Showing form if it is necessary.
 * @param req http request
 * @return
 *      true: valid user, you can continue
 *      false: incorrect user/pass or not authenticated
 */
bool e12aio_httpd_auth_ok(httpd_req_t *req)
{
    if (!e12aio_httpd_auth_valid(req))
    {
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Login Form\"");
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, NULL, 0);
        return false;
    }
    return true;
}

esp_err_t e12aio_httpd_handler_spiffs(httpd_req_t *req)
{
    // Check Authentication first
    if (!e12aio_httpd_auth_ok(req))
        return ESP_OK;

    // Process request
    size_t readBytes = 0;
    char l_filename[E12AIO_MAX_FILENAME];
    char l_buffer[CONFIG_JSON_BUFFER_SIZE];
    snprintf(l_filename, E12AIO_MAX_FILENAME, "/spiffs/%s", (const char *)req->user_ctx);
    ESP_LOGI(TAG, "Serving page file: %s", l_filename);
    FILE *l_fp = fopen(l_filename, "r");
    if (l_fp == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file: %s for reading", l_filename);
        return ESP_ERR_NOT_FOUND;
    }
    do
    {
        readBytes = fread(&l_buffer, 1, CONFIG_JSON_BUFFER_SIZE, l_fp);
        httpd_resp_send_chunk(req, l_buffer, readBytes);
    } while (readBytes == CONFIG_JSON_BUFFER_SIZE);
    fclose(l_fp);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t e12aio_httpd_handler_restart(httpd_req_t *req)
{
    if (!e12aio_httpd_auth_ok(req))
        return ESP_OK;

    static const char *l_static_resp = "{\"status\":\"ok\"}";
    if (e12aio_config_lazy_started())
        e12aio_config_save();
    httpd_resp_send(req, l_static_resp, strlen(l_static_resp));
    vTaskDelay(CONFIG_WEB_WAIT_BEFORE_RESTART / portTICK_PERIOD_MS);
    esp_restart();
    return ESP_OK;
}

esp_err_t e12aio_httpd_handler_status(httpd_req_t *req)
{
    // some 316 bytes a cada chamada (a memoria)
    if (!e12aio_httpd_auth_ok(req))
        return ESP_OK;
    ESP_LOGD(TAG, "Creating status json");
    cJSON *l_json = cJSON_CreateObject();
    cJSON *l_json_status = cJSON_CreateString("ok");
    cJSON_AddItemToObject(l_json, "status", l_json_status);

    // Board Block
    ESP_LOGD(TAG, "Creating status board");
    char l_uptime[E12AIO_UTILS_UPTIME_SIZE];
    e12aio_utils_uptime((char *)&l_uptime, E12AIO_UTILS_UPTIME_SIZE);
    cJSON *l_board = cJSON_CreateObject();
    cJSON *l_board_model = cJSON_CreateString("E12-AIO3");
    cJSON *l_board_hostname = cJSON_CreateString(e12aio_config_get_name());
    cJSON *l_board_build = cJSON_CreateString(E12AIO_VERSION);
    cJSON *l_board_uptime = cJSON_CreateString(l_uptime);
    cJSON_AddItemToObject(l_board, "model", l_board_model);
    cJSON_AddItemToObject(l_board, "hostname", l_board_hostname);
    cJSON_AddItemToObject(l_board, "build", l_board_build);
    cJSON_AddItemToObject(l_board, "uptime", l_board_uptime);
    cJSON_AddItemToObject(l_json, "board", l_board);

    // Wifi block
    ESP_LOGD(TAG, "Creating status wifi");
    cJSON *l_wifi = cJSON_CreateObject();
    cJSON *l_wifi_mode;
    cJSON *l_wifi_status;
    cJSON *l_wifi_ip = cJSON_CreateString(e12aio_wifi_get_ip());
    cJSON *l_wifi_gateway = cJSON_CreateString(e12aio_wifi_get_gateway());
    cJSON *l_wifi_netmask = cJSON_CreateString(e12aio_wifi_get_netmask());
    if (e12aio_wifi_sta_is_connected())
    {
        l_wifi_mode = cJSON_CreateString("STA");
        l_wifi_status = cJSON_CreateString("connected");
    }
    else
    {
        l_wifi_mode = cJSON_CreateString("AP_STA");
        l_wifi_status = cJSON_CreateString("disconnected");
    }
    cJSON_AddItemToObject(l_wifi, "mode", l_wifi_mode);
    cJSON_AddItemToObject(l_wifi, "status", l_wifi_status);
    cJSON_AddItemToObject(l_wifi, "ip", l_wifi_ip);
    cJSON_AddItemToObject(l_wifi, "gateway", l_wifi_gateway);
    cJSON_AddItemToObject(l_wifi, "netmask", l_wifi_netmask);
    cJSON_AddItemToObject(l_json, "wifi", l_wifi);

    // MQTT Block
    ESP_LOGD(TAG, "Creating status mqtt");
    cJSON *l_mqtt = cJSON_CreateObject();
    cJSON *l_mqtt_status = cJSON_CreateString(e12aio_mqtt_is_connected() ? "connected" : "disconnected");
    cJSON_AddItemToObject(l_mqtt, "status", l_mqtt_status);
    cJSON_AddItemToObject(l_json, "mqtt", l_mqtt);

    // Return values (protect memory)
    char l_buffer[CONFIG_JSON_BUFFER_SIZE];
    strncpy(l_buffer, cJSON_Print(l_json), CONFIG_JSON_BUFFER_SIZE);
    cJSON_Delete(l_json);

    ESP_LOGD(TAG, "JSON: %s", l_buffer);
    httpd_resp_send(req, l_buffer, strlen(l_buffer));
    return ESP_OK;
}

esp_err_t e12aio_httpd_handler_relay_set(httpd_req_t *req)
{
    if (!e12aio_httpd_auth_ok(req))
        return ESP_OK;
    char *l_buffer = "{\"status\":\"err\",\"message\":\"under development\"}";
    httpd_resp_send(req, l_buffer, strlen(l_buffer));
    return ESP_OK;
}

char *e12aio_httpd_userctx_add(char *value)
{
    g_userctx_len++;
    g_userctx = realloc(g_userctx, sizeof(char *) * g_userctx_len);
    g_userctx[g_userctx_len - 1] = strdup(value);
    return g_userctx[g_userctx_len - 1];
}

void e12aio_httpd_userctx_clear()
{
    for (; g_userctx_len != 0; g_userctx_len--)
    {
        free(g_userctx[g_userctx_len]);
    }
    free(g_userctx);
    g_userctx = NULL;
}

void e12aio_httpd_register_spiffs()
{
    // Open spiffs directory
    DIR *dir = opendir("/spiffs");
    if (dir == NULL)
    {
        ESP_LOGE(TAG, "Fail to open /spiffs directory.");
        e12aio_httpd_stop();
        return;
    }

    // List spiffs files and register as handler
    struct dirent *de;
    while ((de = readdir(dir)) != NULL)
    {
        char l_uri[E12AIO_MAX_FILENAME];
        snprintf(l_uri, E12AIO_MAX_FILENAME, "/%s", de->d_name);
        ESP_LOGD(TAG, "SPIFF File: %s", de->d_name);
        httpd_uri_t l_uri_config = {
            .uri = l_uri,
            .method = HTTP_GET,
            .handler = e12aio_httpd_handler_spiffs,
            .user_ctx = e12aio_httpd_userctx_add(de->d_name),
        };
        httpd_register_uri_handler(g_server, &l_uri_config);
    }
    closedir(dir);
}

void e12aio_httpd_register_index()
{
    // Register base (/) handler
    httpd_uri_t l_uri_config = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = e12aio_httpd_handler_spiffs,
        .user_ctx = e12aio_httpd_userctx_add("index.html"),
    };
    httpd_register_uri_handler(g_server, &l_uri_config);
}

void e12aio_httpd_register_restart()
{
    // Register base (/) handler
    httpd_uri_t l_uri_config = {
        .uri = "/restart",
        .method = HTTP_GET,
        .handler = e12aio_httpd_handler_restart,
    };
    httpd_register_uri_handler(g_server, &l_uri_config);
}

void e12aio_httpd_register_status()
{
    // Register base (/) handler
    httpd_uri_t l_uri_config = {
        .uri = "/status",
        .method = HTTP_GET,
        .handler = e12aio_httpd_handler_status,
    };
    httpd_register_uri_handler(g_server, &l_uri_config);
}

void e12aio_httpd_register_relay_set()
{
    // Register base (/) handler
    httpd_uri_t l_uri_config = {
        .uri = "/relay",
        .method = HTTP_GET,
        .handler = e12aio_httpd_handler_relay_set,
    };
    httpd_register_uri_handler(g_server, &l_uri_config);
}

void e12aio_httpd_start()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&g_server, &config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        e12aio_httpd_register_spiffs();
        e12aio_httpd_register_index();
        e12aio_httpd_register_restart();
        e12aio_httpd_register_status();
        e12aio_httpd_register_relay_set();
    }
    else
    {
        ESP_LOGI(TAG, "Error starting server!");
    }
}

void e12aio_httpd_stop()
{
    if (g_server != NULL)
    {
        e12aio_httpd_userctx_clear();
        httpd_stop(g_server);
    }
}

#endif