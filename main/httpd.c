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
#include "relay.h"
#include "ota.h"
#include "spiffs.h"

#ifdef CONFIG_COMPONENT_HTTPD
static const char *TAG = "httpd.c";
static httpd_handle_t g_server = NULL;
static const char *g_static_resp = "{\"status\":\"ok\"}";

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

    char l_hdr_buff[l_hdr_len + 1];
    if (httpd_req_get_hdr_value_str(req, "Authorization", l_hdr_buff, l_hdr_len + 1) == ESP_OK)
    {
        char l_decoded[50];
        size_t l_decodedSize;
        char *l_partType = strtok(l_hdr_buff, " ");
        char *l_partAuth64 = strtok(NULL, " ");
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

/**
 * @brief This function handle all request from spiffs pages
 */
esp_err_t e12aio_httpd_handler_spiffs(httpd_req_t *req)
{
    // Check Authentication first
    if (!e12aio_httpd_auth_ok(req))
        return ESP_OK;

    size_t l_size = 0;
    char l_filename[E12AIO_MAX_FILENAME];
    char l_buffer[CONFIG_HTTPD_MAX_URI_LEN];

    // Check file name
    l_size = httpd_req_get_url_query_len(req);
    if (l_size > 0)
    {
        char l_partialFileName[E12AIO_MAX_FILENAME];
        httpd_req_get_url_query_str(req, l_buffer, CONFIG_HTTPD_MAX_URI_LEN);
        httpd_query_key_value(l_buffer, "file", l_partialFileName, E12AIO_MAX_FILENAME);
        snprintf(l_filename, E12AIO_MAX_FILENAME, "%s/%s", e12aio_spiffs_get_basepath(), l_partialFileName);
    }
    else
        strcpy(l_filename, "/v/index.html");

    // Process request
    ESP_LOGI(TAG, "Serving page file: %s", l_filename);
    do
    {
        l_size = e12aio_spiffs_read(l_filename, (char *)&l_buffer, CONFIG_HTTPD_MAX_URI_LEN);
        httpd_resp_send_chunk(req, l_buffer, l_size);
    } while (l_size == CONFIG_HTTPD_MAX_URI_LEN);

    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t e12aio_httpd_handler_save(httpd_req_t *req)
{
    if (!e12aio_httpd_auth_ok(req))
        return ESP_OK;

    char l_buffer[CONFIG_JSON_BUFFER_SIZE];
    httpd_req_recv(req, (char *)&l_buffer, CONFIG_JSON_BUFFER_SIZE);
    l_buffer[req->content_len] = 0;
    e12aio_config_load_from_buffer(l_buffer);
    e12aio_config_save();
    httpd_resp_send(req, l_buffer, strlen(l_buffer));
    return ESP_OK;
}

/**
 * @brief Send a status json to client
 * Sample: { "status": "ok", "board": { "model": "E12-AIO3", "hostname": "e12aio_CC8C20", "build": "2.0.0", "uptime": "00:11:52" }, "wifi": { "mode": "STA", "status": "connected", "ip": "10.15.197.199", "gateway": "10.15.20.1", "netmask": "255.255.0.0" }, "mqtt": { "status": "connected" } }
 */

esp_err_t e12aio_httpd_handler_status(httpd_req_t *req)
{
    // some 316 bytes a cada chamada (a memoria)
    if (!e12aio_httpd_auth_ok(req))
        return ESP_OK;

    cJSON *l_json = cJSON_CreateObject();
    cJSON *l_json_status = cJSON_CreateString("ok");
    cJSON_AddItemToObject(l_json, "status", l_json_status);

    // Board Block
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
    cJSON *l_mqtt = cJSON_CreateObject();
    cJSON *l_mqtt_status = cJSON_CreateString(e12aio_mqtt_is_connected() ? "connected" : "disconnected");
    cJSON_AddItemToObject(l_mqtt, "status", l_mqtt_status);
    cJSON_AddItemToObject(l_json, "mqtt", l_mqtt);

    // Relay Block
    cJSON *l_relay = cJSON_CreateArray();
    cJSON *l_port1 = cJSON_CreateBool(e12aio_config_get()->relay.port1);
    cJSON *l_port2 = cJSON_CreateBool(e12aio_config_get()->relay.port2);
    cJSON *l_port3 = cJSON_CreateBool(e12aio_config_get()->relay.port3);
    cJSON_AddItemToArray(l_relay, l_port1);
    cJSON_AddItemToArray(l_relay, l_port2);
    cJSON_AddItemToArray(l_relay, l_port3);
    cJSON_AddItemToObject(l_json, "relay", l_relay);

    // Return values (protect memory)
    char l_buffer[CONFIG_JSON_BUFFER_SIZE];
    cJSON_PrintPreallocated(l_json, (char *)&l_buffer, CONFIG_JSON_BUFFER_SIZE, false);
    cJSON_Delete(l_json);
    httpd_resp_send(req, l_buffer, strlen(l_buffer));
    return ESP_OK;
}

esp_err_t e12aio_httpd_handler_api(httpd_req_t *req)
{
    // Check file name
    size_t l_size = httpd_req_get_url_query_len(req);
    char l_buffer[l_size + 1];
    char l_token[CONFIG_WEB_AUTH_MAX_SIZE];
    httpd_req_get_url_query_str(req, (char *)&l_buffer, l_size + 1);
    httpd_query_key_value(l_buffer, "token", (char *)&l_token, CONFIG_WEB_AUTH_MAX_SIZE);
    if (strncmp(l_token, e12aio_config_get()->httpd.token,
                strlen(e12aio_config_get()->httpd.token)) == 0)
    {
        char l_relay_id[2];
        char l_relay_status[4];
        httpd_query_key_value(l_buffer, "port", (char *)&l_relay_id, 2);
        httpd_query_key_value(l_buffer, "status", (char *)&l_relay_status, 4);
        e12aio_relay_set(atoi(l_relay_id), strncmp(l_relay_status, "ON", 2) == 0);
        httpd_resp_send(req, g_static_resp, strlen(g_static_resp));
        return ESP_OK;
    }
    httpd_resp_set_status(req, "401 Not Authorized");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/**
 * @brief Handle all /action
 */
esp_err_t e12aio_httpd_handler_action(httpd_req_t *req)
{
    if (!e12aio_httpd_auth_ok(req))
        return ESP_OK;

    // Check file name
    size_t l_size = httpd_req_get_url_query_len(req);
    char l_buffer[l_size + 1];
    char l_name[15];

    httpd_req_get_url_query_str(req, (char *)&l_buffer, l_size + 1);
    httpd_query_key_value(l_buffer, "name", (char *)&l_name, 15);
    if (strncmp(l_name, "restart", 7) == 0)
    {
        // Restart block
        if (e12aio_config_lazy_started())
            e12aio_config_save();
        httpd_resp_send(req, g_static_resp, strlen(g_static_resp));
        vTaskDelay(CONFIG_WEB_WAIT_BEFORE_RESTART / portTICK_PERIOD_MS);
        esp_restart();
        return ESP_OK;
    }
    else if (strncmp(l_name, "relay", 5) == 0)
    {
#ifdef CONFIG_COMPONENT_RELAY
        char l_relay_id[2];
        char l_relay_status[4];
        httpd_query_key_value(l_buffer, "id", (char *)&l_relay_id, 2);
        httpd_query_key_value(l_buffer, "status", (char *)&l_relay_status, 4);
        e12aio_relay_set(atoi(l_relay_id), strncmp(l_relay_status, "ON", 2) == 0);
        httpd_resp_send(req, g_static_resp, strlen(g_static_resp));
        return ESP_OK;
#endif
    }
    else if (strncmp(l_name, "firmware", 8) == 0)
    {
#ifdef CONFIG_COMPONENT_OTA
        char l_url[E12AIO_OTA_URL_SIZE];
        httpd_query_key_value(l_buffer, "url", (char *)&l_url, E12AIO_OTA_URL_SIZE);
        //TODO: Convert from GET to POST (permitting a use of larger URLs like GitHub, OneDrive, etc.).
        e12aio_ota_file_write_url_firmware(l_url, strlen(l_url));
        e12aio_ota_file_write_url_firmware(NULL, 0);
        e12aio_ota_start();
        httpd_resp_send(req, g_static_resp, strlen(g_static_resp));
        return ESP_OK;
#endif
    }
    httpd_resp_set_status(req, "500 Server Error");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/**
 * @brief Handle all pages on spiffs
 * 
 * Sample calls:
 * /
 * /?file=script.js
 * 
 */
void e12aio_httpd_register_index()
{
    // Register base (/) handler
    httpd_uri_t l_uri_config = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = e12aio_httpd_handler_spiffs,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(g_server, &l_uri_config);
}

/**
 * @brief Handle /save page.
 * This page is responsible to update config.json file on spiffs.
 */
void e12aio_httpd_register_save()
{
    // Register base (/) handler
    httpd_uri_t l_uri_config = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = e12aio_httpd_handler_save,
    };
    httpd_register_uri_handler(g_server, &l_uri_config);
}

/**
 * @brief Handle /status page
 * This page is a json with "running" configurations.
 */
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

/**
 * @brief Handle /action page
 * 
 * Sample calls:
 * /action?name=restart
 * /action?name=relay&id=1&status=ON
 * /action?name=firmware&url=http://github.com/example
 */
void e12aio_httpd_register_action()
{
    // Register base (/action) handler
    httpd_uri_t l_uri_config = {
        .uri = "/action",
        .method = HTTP_GET,
        .handler = e12aio_httpd_handler_action,
    };
    httpd_register_uri_handler(g_server, &l_uri_config);
}

/**
 * @brief Handle /api page
 * 
 * Sample calls:
 * /api?token=123456789&port=1&status=ON
 */
void e12aio_httpd_register_api()
{
    // Register base (/action) handler
    httpd_uri_t l_uri_config = {
        .uri = "/api",
        .method = HTTP_GET,
        .handler = e12aio_httpd_handler_api,
    };
    httpd_register_uri_handler(g_server, &l_uri_config);
}

/**
 * @brief Start httpd component
 */
void e12aio_httpd_start()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&g_server, &config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        e12aio_httpd_register_index();
        e12aio_httpd_register_action();
        e12aio_httpd_register_status();
        e12aio_httpd_register_save();
        e12aio_httpd_register_api();
    }
    else
    {
        ESP_LOGI(TAG, "Error starting server!");
    }
}

/**
 * @brief Stop the httpd component
 */
void e12aio_httpd_stop()
{
    if (g_server != NULL)
        httpd_stop(g_server);
    g_server = NULL;
}

#endif