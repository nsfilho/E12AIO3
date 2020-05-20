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
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <sys/stat.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <esp_log.h>
#include <esp_spiffs.h>
#include <cJSON.h>
#include <string.h>
#include "config.h"

const char *TAG = "config.cpp";
const char *g_config_file = "/v/config.json";

static e12aio_config_t g_config;
static EventGroupHandle_t g_eventGroup;

void e12aio_config_init_task(void *arg)
{
    e12aio_config_prepare_spiffs();
    e12aio_config_prepare_configs();
    xEventGroupSetBits(g_eventGroup, E12AIO_CONFIG_LOADED);
    vTaskDelete(NULL);
}

void e12aio_config_init()
{
    static bool s_initialized = false;
    if (!s_initialized)
    {
        s_initialized = true;
        g_eventGroup = xEventGroupCreate();
        xTaskCreate(e12aio_config_init_task, "config_init", 2048, NULL, 15, NULL);
    }
    else
    {
        xEventGroupWaitBits(g_eventGroup, E12AIO_CONFIG_LOADED, pdFALSE, pdTRUE, portMAX_DELAY);
    }
}

/**
 * Generate and return the esp chipid name
 */
char *e12aio_config_get_name()
{
    static bool s_prepared = false;
    static char s_esp_name[14];
    if (s_prepared)
        return (char *)&s_esp_name;

    uint8_t l_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, l_mac);
    snprintf(s_esp_name, sizeof(s_esp_name), "e12aio_%02X%02X%02X", l_mac[3], l_mac[4], l_mac[5]);
    ESP_LOGD(TAG, "ESP Name: %s", s_esp_name);
    s_prepared = true;
    return (char *)&s_esp_name;
}

/**
 * Prepare spiffs structure do save and load data
 */
void e12aio_config_prepare_spiffs()
{
    ESP_LOGI(TAG, "Initializing SPIFFS");
    esp_vfs_spiffs_conf_t l_conf = {
        .base_path = "/v",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true,
    };

    esp_err_t l_ret = esp_vfs_spiffs_register(&l_conf);
    if (l_ret != ESP_OK)
    {
        if (l_ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (l_ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(l_ret));
        }
        return;
    }
}

/**
 * Prepare your configuration or set default values
 */
void e12aio_config_prepare_configs()
{
    struct stat l_st;
    if (stat(g_config_file, &l_st) == 0)
    {
        // loading from system
        e12aio_config_load();
    }
    else
    {
        // Initialize the configuration with fabric defaults
        e12aio_config_load_from_buffer("{}");
        e12aio_config_lazy_save();
    }
}

/**
 * Save all your configurations in a lazy mode (this is a auxiliary task).
 */
void e12aio_config_lazy_save_task(void *args)
{
    vTaskDelay(CONFIG_SAVE_LAZY_TIMEOUT / portTICK_PERIOD_MS);
    if (xEventGroupGetBits(g_eventGroup) & E12AIO_CONFIG_DELAYED_SAVE)
    {
        e12aio_config_save();
        ESP_LOGD(TAG, "Config saved from lazy call");
    }
    vTaskDelete(NULL);
}

/**
 * Save all your configurations in a lazy mode (avoid to many simultaenous write)
 */
void e12aio_config_lazy_save()
{
    if (!(xEventGroupGetBits(g_eventGroup) & E12AIO_CONFIG_DELAYED_SAVE))
    {
        ESP_LOGI(TAG, "Creating a lazy saving task");
        xEventGroupSetBits(g_eventGroup, E12AIO_CONFIG_DELAYED_SAVE);
        xTaskCreate(e12aio_config_lazy_save_task, "config_lazy_save", 2048, NULL, 1, NULL);
    }
    else
    {
        ESP_LOGD(TAG, "A lazy saving was already invoked...");
    }
}

void e12aio_config_load()
{
    // Config file exists
    ESP_LOGI(TAG, "Reading config file: %s", g_config_file);
    char l_buffer[CONFIG_JSON_BUFFER_SIZE];
    FILE *l_fp = fopen(g_config_file, "r");
    if (l_fp == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file: %s for reading", g_config_file);
        return;
    }
    fread(&l_buffer, 1, CONFIG_JSON_BUFFER_SIZE, l_fp);
    fclose(l_fp);
    e12aio_config_load_from_buffer(l_buffer);
}

void e12aio_config_load_from_buffer(const char *buffer)
{
    // parse file
    cJSON *l_json = cJSON_Parse(buffer);

    // WIFI Config
    cJSON *l_wifi = cJSON_GetObjectItem(l_json, "wifi");
    cJSON *l_wifi_ssid = NULL;
    cJSON *l_wifi_password = NULL;
    if (l_wifi != NULL)
    {
        l_wifi_ssid = cJSON_GetObjectItem(l_wifi, "ssid");
        l_wifi_password = cJSON_GetObjectItem(l_wifi, "password");
    }
    strncpy(g_config.wifi.ssid, l_wifi_ssid != NULL ? l_wifi_ssid->valuestring : CONFIG_WIFI_SSID, 32);
    strncpy(g_config.wifi.password, l_wifi_password != NULL ? l_wifi_password->valuestring : CONFIG_WIFI_PASSWORD, 64);

    // MQTT Config
    cJSON *l_mqtt = cJSON_GetObjectItem(l_json, "mqtt");
    cJSON *l_mqtt_url = NULL;
    cJSON *l_mqtt_topic = NULL;
    if (l_mqtt != NULL)
    {
        l_mqtt_url = cJSON_GetObjectItem(l_mqtt, "url");
        l_mqtt_topic = cJSON_GetObjectItem(l_mqtt, "topic");
    }
    strncpy(g_config.mqtt.url, l_mqtt_url != NULL ? l_mqtt_url->valuestring : CONFIG_MQTT_URL, 100);
    strncpy(g_config.mqtt.topic, l_mqtt_topic != NULL ? l_mqtt_topic->valuestring : CONFIG_MQTT_TOPIC_BASE, CONFIG_MQTT_TOPIC_SIZE);

    // Relay Config
    cJSON *l_relay = cJSON_GetObjectItem(l_json, "relay");
    g_config.relay.port1 = false;
    g_config.relay.port2 = false;
    g_config.relay.port3 = false;
    if (l_relay != NULL)
    {
        cJSON *l_status = NULL;
        uint8_t l_relay_counter = 1;
        cJSON_ArrayForEach(l_status, l_relay)
        {
            switch (l_relay_counter)
            {
            case 1:
                g_config.relay.port1 = cJSON_IsTrue(l_status);
                break;
            case 2:
                g_config.relay.port2 = cJSON_IsTrue(l_status);
                break;
            case 3:
                g_config.relay.port3 = cJSON_IsTrue(l_status);
                break;
            }
            l_relay_counter++;
        }
    }

    // HTTPD Config
    cJSON *l_httpd = cJSON_GetObjectItem(l_json, "httpd");
    cJSON *l_httpd_username = NULL;
    cJSON *l_httpd_password = NULL;
    cJSON *l_httpd_token = NULL;
    if (l_httpd != NULL)
    {
        l_httpd_username = cJSON_GetObjectItem(l_httpd, "username");
        l_httpd_password = cJSON_GetObjectItem(l_httpd, "password");
        l_httpd_token = cJSON_GetObjectItem(l_httpd, "token");
    }
    strncpy(g_config.httpd.username, l_httpd_username != NULL ? l_httpd_username->valuestring : CONFIG_WEB_AUTH_USERNAME, CONFIG_WEB_AUTH_MAX_SIZE);
    strncpy(g_config.httpd.password, l_httpd_password != NULL ? l_httpd_password->valuestring : CONFIG_WEB_AUTH_PASSWORD, CONFIG_WEB_AUTH_MAX_SIZE);
    strncpy(g_config.httpd.token, l_httpd_token != NULL ? l_httpd_token->valuestring : CONFIG_WEB_TOKEN, CONFIG_WEB_AUTH_MAX_SIZE);

    // OTA Config
    cJSON *l_ota = cJSON_GetObjectItem(l_json, "ota");
    cJSON *l_ota_state = NULL;
    cJSON *l_ota_url = NULL;
    cJSON *l_ota_version = NULL;
    if (l_ota != NULL)
    {
        l_ota_state = cJSON_GetObjectItem(l_ota, "state");
        l_ota_url = cJSON_GetObjectItem(l_ota, "url");
        l_ota_version = cJSON_GetObjectItem(l_ota, "version");
    }
    g_config.ota.state = (l_ota_state != NULL ? l_ota_state->valueint : E12AIO_OTA_OK);
    strncpy(g_config.ota.url, l_ota_url != NULL ? l_ota_url->valuestring : "", E12AIO_OTA_URL_SIZE);
    strncpy(g_config.ota.version, l_ota_version != NULL ? l_ota_version->valuestring : "undefined", E12AIO_OTA_VERSION_SIZE);

    ESP_LOGI(TAG, "Config loaded!");
    cJSON_Delete(l_json);
}

/**
 * Save configuration in SPIFFS
 */
void e12aio_config_save()
{
    char l_buffer[CONFIG_JSON_BUFFER_SIZE];
    FILE *l_fp = fopen(g_config_file, "w+");
    if (l_fp == NULL)
    {
        ESP_LOGE(TAG, "Failed to write file: %s", g_config_file);
        return;
    }
    size_t l_len = e12aio_config_save_buffer_adv(g_config, l_buffer, CONFIG_JSON_BUFFER_SIZE);
    fwrite(l_buffer, 1, l_len, l_fp);
    fclose(l_fp);
    xEventGroupClearBits(g_eventGroup, E12AIO_CONFIG_DELAYED_SAVE);
}

size_t e12aio_config_save_buffer(char *buffer, size_t sz)
{
    return e12aio_config_save_buffer_adv(g_config, buffer, sz);
}

size_t e12aio_config_save_buffer_adv(e12aio_config_t data, char *buffer, size_t sz)
{
    // Generate a clear config.json
    cJSON *json = cJSON_CreateObject();
    cJSON *wifi = cJSON_CreateObject();
    cJSON *wifi_ssid = cJSON_CreateString(data.wifi.ssid);
    cJSON *wifi_password = cJSON_CreateString(data.wifi.password);
    cJSON_AddItemToObject(wifi, "ssid", wifi_ssid);
    cJSON_AddItemToObject(wifi, "password", wifi_password);
    cJSON_AddItemToObject(json, "wifi", wifi);
    cJSON *mqtt = cJSON_CreateObject();
    cJSON *mqtt_url = cJSON_CreateString(data.mqtt.url);
    cJSON *mqtt_topic = cJSON_CreateString(data.mqtt.topic);
    cJSON_AddItemToObject(mqtt, "url", mqtt_url);
    cJSON_AddItemToObject(mqtt, "topic", mqtt_topic);
    cJSON_AddItemToObject(json, "mqtt", mqtt);
    cJSON *relay = cJSON_CreateArray();
    cJSON *port1 = cJSON_CreateBool(g_config.relay.port1);
    cJSON *port2 = cJSON_CreateBool(g_config.relay.port2);
    cJSON *port3 = cJSON_CreateBool(g_config.relay.port3);
    cJSON_AddItemToArray(relay, port1);
    cJSON_AddItemToArray(relay, port2);
    cJSON_AddItemToArray(relay, port3);
    cJSON_AddItemToObject(json, "relay", relay);
    cJSON *httpd = cJSON_CreateObject();
    cJSON *username = cJSON_CreateString(data.httpd.username);
    cJSON *password = cJSON_CreateString(data.httpd.password);
    cJSON *token = cJSON_CreateString(data.httpd.token);
    cJSON_AddItemToObject(httpd, "username", username);
    cJSON_AddItemToObject(httpd, "password", password);
    cJSON_AddItemToObject(httpd, "token", token);
    cJSON_AddItemToObject(json, "httpd", httpd);
    cJSON *l_ota = cJSON_CreateObject();
    cJSON *l_ota_state = cJSON_CreateNumber(g_config.ota.state);
    cJSON *l_ota_url = cJSON_CreateString(g_config.ota.url);
    cJSON *l_ota_version = cJSON_CreateString(g_config.ota.version);
    cJSON_AddItemToObject(l_ota, "state", l_ota_state);
    cJSON_AddItemToObject(l_ota, "url", l_ota_url);
    cJSON_AddItemToObject(l_ota, "version", l_ota_version);
    cJSON_AddItemToObject(json, "ota", l_ota);
    cJSON_PrintPreallocated(json, buffer, sz, false);
    cJSON_Delete(json);
    return strlen(buffer);
}

void e12aio_config_dump()
{
    ESP_LOGD(TAG, "WIFI - SSID: %s", g_config.wifi.ssid);
    ESP_LOGD(TAG, "WIFI - Password: %s", g_config.wifi.password);
    ESP_LOGD(TAG, "MQTT - URL: %s", g_config.mqtt.url);
    ESP_LOGD(TAG, "MQTT - TOPIC: %s", g_config.mqtt.topic);
    ESP_LOGD(TAG, "Relay - Port 1: %s", g_config.relay.port1 ? "true" : "false");
    ESP_LOGD(TAG, "Relay - Port 2: %s", g_config.relay.port2 ? "true" : "false");
    ESP_LOGD(TAG, "Relay - Port 3: %s", g_config.relay.port3 ? "true" : "false");
}

void e12aio_config_wait_load(const char *TAG)
{
    ESP_LOGD(TAG, "waiting until all configuration is loaded...");
    xEventGroupWaitBits(g_eventGroup, E12AIO_CONFIG_LOADED, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGD(TAG, "done, configuration was loaded...");
}

bool e12aio_config_lazy_started()
{
    return (xEventGroupGetBits(g_eventGroup) & E12AIO_CONFIG_DELAYED_SAVE);
}

e12aio_config_t *e12aio_config_get()
{
    return &g_config;
}

void e12aio_config_update_from_buffer(const char *buffer)
{
    e12aio_config_load_from_buffer(buffer);
    e12aio_config_save();
}

bool *e12aio_config_relay_pointer(uint8_t relay)
{
    bool *l_port = NULL;
    switch (relay)
    {
    case 1:
        l_port = &g_config.relay.port1;
        break;
    case 2:
        l_port = &g_config.relay.port2;
        break;
    case 3:
        l_port = &g_config.relay.port2;
        break;
    }
    return l_port;
}

void e12aio_config_relay_set(uint8_t relay, bool status)
{
    e12aio_config_relay_set_adv(relay, status, true);
}

void e12aio_config_relay_set_adv(uint8_t relay, bool status, bool save)
{
    bool *l_port = e12aio_config_relay_pointer(relay);
    if (*l_port != status)
    {
        *l_port = status;
        if (save)
            e12aio_config_lazy_save();
    }
}