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
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "config.hpp"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "cJSON.h"
#include "sdkconfig.h"

const char *TAG = "config.cpp";
const char *g_config_file = "/spiffs/config.json";

static std::string g_name;
static config_t g_config;
static EventGroupHandle_t g_eventGroup;
static bool g_initialized = false;

ConfigClass::ConfigClass()
{
}

void ConfigClass::init()
{
    if (!g_initialized)
    {
        g_initialized = true;
        g_eventGroup = xEventGroupCreate();
        this->prepare_name();
        this->prepare_spiffs();
        this->prepare_configs();
        xEventGroupSetBits(g_eventGroup, D_CONFIG_LOADED);
        ESP_LOGD(TAG, "Config was loaded!");
    }
}

/**
 * Generate esp name
 */
void ConfigClass::prepare_name()
{
    uint8_t l_mac[6];
    char l_esp_name[33];
    esp_wifi_get_mac(WIFI_IF_STA, l_mac);
    snprintf(l_esp_name, sizeof(l_esp_name), "e12aio_%02X%02X%02X", l_mac[3], l_mac[4], l_mac[5]);
    g_name = l_esp_name;
    ESP_LOGD(TAG, "ESP Name: %s", l_esp_name);
}

/**
 * Return ESP Name
 */
std::string ConfigClass::getName()
{
    return g_name;
}

/**
 * Prepare spiffs structure do save and load data
 */
void ConfigClass::prepare_spiffs()
{
    ESP_LOGI(TAG, "Initializing SPIFFS...");
    esp_vfs_spiffs_conf_t l_conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 15,
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
void ConfigClass::prepare_configs()
{
    struct stat l_st;
    if (stat(g_config_file, &l_st) == 0)
    {
        // loading from system
        this->load();
    }
    else
    {
        // Initialize the configuration with fabric defaults
        this->loadInMemory("{}");
        this->lazySave();
    }
}

/**
 * Save all your configurations in a lazy mode (this is a auxiliary task).
 */
void config_lazy_save_task(void *args)
{
    vTaskDelay(CONFIG_SAVE_LAZY_TIMEOUT / portTICK_PERIOD_MS);
    Config.save();
    ESP_LOGD(TAG, "Config saved (lazy)!");
    vTaskDelete(NULL);
}

/**
 * Save all your configurations in a lazy mode (avoid to many simultaenous write)
 */
void ConfigClass::lazySave()
{
    if (!(xEventGroupGetBits(g_eventGroup) & D_CONFIG_DELAYED_SAVE))
    {
        ESP_LOGD(TAG, "Creating a lazy saving task...");
        xEventGroupSetBits(g_eventGroup, D_CONFIG_DELAYED_SAVE);
        xTaskCreate(config_lazy_save_task, "config_lazy_save", 2048, NULL, 1, NULL);
    }
    else
    {
        ESP_LOGD(TAG, "A lazy saving was already started invoked...");
    }
}

void ConfigClass::load()
{
    // Config file exists
    ESP_LOGI(TAG, "Reading file: /spiffs/config.json");
    char l_buffer[CONFIG_JSON_BUFFER_SIZE];
    FILE *l_fp = fopen(g_config_file, "r");
    if (l_fp == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file: %s for reading", g_config_file);
        return;
    }
    fread(&l_buffer, 1, CONFIG_JSON_BUFFER_SIZE, l_fp);
    fclose(l_fp);
    this->loadInMemory(l_buffer);
}

void ConfigClass::loadInMemory(const char *buffer)
{
    // parse file
    cJSON *l_json = cJSON_Parse(buffer);
    cJSON *l_wifi = cJSON_GetObjectItem(l_json, "wifi");
    if (l_wifi != NULL)
    {
        cJSON *l_wifi_ssid = cJSON_GetObjectItem(l_wifi, "ssid");
        cJSON *l_wifi_password = cJSON_GetObjectItem(l_wifi, "password");
        g_config.wifi.ssid = l_wifi_ssid != NULL ? l_wifi_ssid->valuestring : CONFIG_WIFI_SSID;
        g_config.wifi.password = l_wifi_password != NULL ? l_wifi_password->valuestring : CONFIG_WIFI_PASSWORD;
    }
    else
    {
        g_config.wifi.ssid = CONFIG_WIFI_SSID;
        g_config.wifi.password = CONFIG_WIFI_PASSWORD;
    }
    cJSON *l_mqtt = cJSON_GetObjectItem(l_json, "mqtt");
    if (l_mqtt != NULL)
    {
        cJSON *l_mqtt_url = cJSON_GetObjectItem(l_mqtt, "url");
        cJSON *l_mqtt_topic = cJSON_GetObjectItem(l_mqtt, "topic");
        g_config.mqtt.url = l_mqtt_url != NULL ? l_mqtt_url->valuestring : CONFIG_MQTT_URL;
        g_config.mqtt.topic = l_mqtt_topic != NULL ? l_mqtt_topic->valuestring : CONFIG_MQTT_TOPIC_BASE;
    }
    else
    {
        g_config.mqtt.url = CONFIG_MQTT_URL;
        g_config.mqtt.topic = CONFIG_MQTT_TOPIC_BASE;
    }

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
    ESP_LOGI(TAG, "Config loaded!");
    cJSON_Delete(l_json);
}

/**
 * Save configuration in SPIFFS
 */
void ConfigClass::save()
{
    char l_buffer[CONFIG_JSON_BUFFER_SIZE];
    FILE *l_fp = fopen(g_config_file, "w+");
    if (l_fp == NULL)
    {
        ESP_LOGE(TAG, "Failed to write file: %s", g_config_file);
        return;
    }
    this->saveInMemory(g_config, l_buffer, CONFIG_JSON_BUFFER_SIZE);
    fprintf(l_fp, "%s", l_buffer);
    fclose(l_fp);
    xEventGroupClearBits(g_eventGroup, D_CONFIG_DELAYED_SAVE);
}

size_t ConfigClass::saveInMemory(config_t data, char *buffer, size_t sz)
{
    // Generate a clear config.json
    cJSON *json = cJSON_CreateObject();
    cJSON *wifi = cJSON_CreateObject();
    cJSON *wifi_ssid = cJSON_CreateString(data.wifi.ssid.c_str());
    cJSON *wifi_password = cJSON_CreateString(data.wifi.password.c_str());
    cJSON_AddItemToObject(wifi, "ssid", wifi_ssid);
    cJSON_AddItemToObject(wifi, "password", wifi_password);
    cJSON_AddItemToObject(json, "wifi", wifi);
    cJSON *mqtt = cJSON_CreateObject();
    cJSON *mqtt_url = cJSON_CreateString(data.mqtt.url.c_str());
    cJSON *mqtt_topic = cJSON_CreateString(data.mqtt.topic.c_str());
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
    const uint16_t l_size = snprintf(buffer, sz, "%s", cJSON_Print(json));
    cJSON_Delete(json);
    return l_size;
}

void ConfigClass::dump()
{
    ESP_LOGI(TAG, "WIFI - SSID: %s", g_config.wifi.ssid.c_str());
    ESP_LOGI(TAG, "WIFI - Password: %s", g_config.wifi.password.c_str());
    ESP_LOGI(TAG, "MQTT - URL: %s", g_config.mqtt.url.c_str());
    ESP_LOGI(TAG, "MQTT - TOPIC: %s", g_config.mqtt.topic.c_str());
    ESP_LOGI(TAG, "Relay - Port 1: %s", g_config.relay.port1 ? "true" : "false");
    ESP_LOGI(TAG, "Relay - Port 2: %s", g_config.relay.port2 ? "true" : "false");
    ESP_LOGI(TAG, "Relay - Port 3: %s", g_config.relay.port3 ? "true" : "false");
}

void ConfigClass::waitUntilLoad(const char *TAG)
{
    ESP_LOGD(TAG, "waiting all configuration has loaded...");
    xEventGroupWaitBits(g_eventGroup, D_CONFIG_LOADED, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGD(TAG, "done! all configs was loaded...");
}

config_t ConfigClass::getValues()
{
    return g_config;
}

void ConfigClass::updateValues(const char *buffer)
{
    this->loadInMemory(buffer);
    this->save();
}

void ConfigClass::setRelayStatus(uint8_t relay, bool status)
{
    switch (relay)
    {
    case 1:
        g_config.relay.port1 = status;
        break;
    case 2:
        g_config.relay.port2 = status;
        break;
    case 3:
        g_config.relay.port3 = status;
        break;
    }
}

ConfigClass Config;