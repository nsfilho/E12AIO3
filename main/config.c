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
#include <cJSON.h>
#include <string.h>
#include "e12aio.h"
#include "config.h"
#include "spiffs.h"

const char *TAG = "config.c";
const char *g_config_file = "/v/config.json";

static e12aio_config_t g_config;
static EventGroupHandle_t g_eventGroup;

void e12aio_config_init_task(void *arg)
{
    e12aio_spiffs_init();
    e12aio_config_prepare_configs();
    xEventGroupSetBits(g_eventGroup, E12AIO_CONFIG_LOADED);
    xEventGroupClearBits(g_eventGroup, E12AIO_CONFIG_DELAYED_SAVE_RIGHTNOW);
    vTaskDelete(NULL);
}

void e12aio_config_init()
{
    static bool s_initialized = false;
    if (!s_initialized)
    {
        s_initialized = true;
        g_eventGroup = xEventGroupCreate();
        xTaskCreate(e12aio_config_init_task, "config_init", 3072, NULL, 15, NULL);
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
 * Prepare your configuration or set default values
 */
void e12aio_config_prepare_configs()
{
    char l_buffer[CONFIG_JSON_BUFFER_SIZE];
    if (e12aio_spiffs_read(g_config_file, (char *)&l_buffer, CONFIG_JSON_BUFFER_SIZE) > 0)
    {
        // initialize using last saved version
        e12aio_config_load_from_buffer(l_buffer);
    }
    else
    {
        // Initialize the configuration with firmware defaults (from fabric)
        e12aio_config_load_from_buffer("{}");
        e12aio_config_lazy_save();
    }
}

/**
 * Save all your configurations in a lazy mode (this is a auxiliary task).
 */
void e12aio_config_lazy_save_task(void *args)
{
    unsigned long time = (unsigned long)args;
    ESP_LOGD(TAG, "Waiting time: %lu", time);
    xEventGroupWaitBits(g_eventGroup, E12AIO_CONFIG_DELAYED_SAVE_RIGHTNOW, pdTRUE, pdTRUE, time / portTICK_PERIOD_MS);
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
    e12aio_config_lazy_save_after(CONFIG_SAVE_LAZY_TIMEOUT);
}

/**
 * Save all your configurations in a lazy mode (avoid to many simultaenous write) after some delay milliseconds
 */
void e12aio_config_lazy_save_after(const unsigned long delay)
{
    if (!e12aio_config_lazy_started())
    {
        ESP_LOGI(TAG, "Creating a lazy saving task: %lu", delay);
        xEventGroupSetBits(g_eventGroup, E12AIO_CONFIG_DELAYED_SAVE);
        xEventGroupClearBits(g_eventGroup, E12AIO_CONFIG_DELAYED_SAVE_RIGHTNOW);
        xTaskCreate(e12aio_config_lazy_save_task, "config_lazy_save", 4096, (void *)delay, 1, NULL);
    }
    else if (delay == 0)
    {
        // make save right now!
        ESP_LOGD(TAG, "Invoked to save right now");
        xEventGroupSetBits(g_eventGroup, E12AIO_CONFIG_DELAYED_SAVE_RIGHTNOW);
    }
    else
    {
        ESP_LOGD(TAG, "A lazy saving was already invoked");
    }
}

void e12aio_config_load_from_buffer(const char *buffer)
{
    // parse file
    cJSON *l_json = cJSON_Parse(buffer);

    // WIFI Config
    cJSON *l_wifi = cJSON_GetObjectItem(l_json, "wifi");
    cJSON *l_wifi_sta_ssid = NULL;
    cJSON *l_wifi_sta_password = NULL;
    cJSON *l_wifi_ap_password = NULL;
    cJSON *l_wifi_dhcp = NULL;
    cJSON *l_wifi_ip = NULL;
    cJSON *l_wifi_netmask = NULL;
    cJSON *l_wifi_gateway = NULL;
    cJSON *l_wifi_dns1 = NULL;
    cJSON *l_wifi_dns2 = NULL;
    if (l_wifi != NULL)
    {
        l_wifi_sta_ssid = cJSON_GetObjectItem(l_wifi, "sta_ssid");
        l_wifi_sta_password = cJSON_GetObjectItem(l_wifi, "sta_password");
        l_wifi_ap_password = cJSON_GetObjectItem(l_wifi, "ap_password");
        l_wifi_dhcp = cJSON_GetObjectItem(l_wifi, "dhcp");
        l_wifi_ip = cJSON_GetObjectItem(l_wifi, "ip");
        l_wifi_netmask = cJSON_GetObjectItem(l_wifi, "netmask");
        l_wifi_gateway = cJSON_GetObjectItem(l_wifi, "gateway");
        l_wifi_dns1 = cJSON_GetObjectItem(l_wifi, "dns1");
        l_wifi_dns2 = cJSON_GetObjectItem(l_wifi, "dns2");
    }
    strncpy(g_config.wifi.sta_ssid, l_wifi_sta_ssid != NULL ? l_wifi_sta_ssid->valuestring : CONFIG_WIFI_SSID, E12AIO_WIFI_SSID_SIZE);
    strncpy(g_config.wifi.sta_password, l_wifi_sta_password != NULL ? l_wifi_sta_password->valuestring : CONFIG_WIFI_PASSWORD, E12AIO_WIFI_PASSWORD_SIZE);
    strncpy(g_config.wifi.ap_password, l_wifi_ap_password != NULL ? l_wifi_ap_password->valuestring : CONFIG_WIFI_AP_PASSWORD, E12AIO_WIFI_PASSWORD_SIZE);
    g_config.wifi.dhcp = (l_wifi_dhcp != NULL) ? cJSON_IsTrue(l_wifi_dhcp) : true;
    strncpy(g_config.wifi.ip, l_wifi_ip != NULL ? l_wifi_ip->valuestring : "192.168.1.2", E12AIO_WIFI_PASSWORD_SIZE);
    strncpy(g_config.wifi.netmask, l_wifi_netmask != NULL ? l_wifi_netmask->valuestring : "255.255.255.0", E12AIO_IPV4_SIZE);
    strncpy(g_config.wifi.gateway, l_wifi_gateway != NULL ? l_wifi_gateway->valuestring : "192.168.1.1", E12AIO_IPV4_SIZE);
    strncpy(g_config.wifi.dns1, l_wifi_dns1 != NULL ? l_wifi_dns1->valuestring : "8.8.8.8", E12AIO_IPV4_SIZE);
    strncpy(g_config.wifi.dns2, l_wifi_dns2 != NULL ? l_wifi_dns2->valuestring : "8.8.4.4", E12AIO_IPV4_SIZE);

    // MQTT Config
    cJSON *l_mqtt = cJSON_GetObjectItem(l_json, "mqtt");
    cJSON *l_mqtt_enable = NULL;
    cJSON *l_mqtt_url = NULL;
    cJSON *l_mqtt_topic = NULL;
    if (l_mqtt != NULL)
    {
        l_mqtt_enable = cJSON_GetObjectItem(l_mqtt, "enable");
        l_mqtt_url = cJSON_GetObjectItem(l_mqtt, "url");
        l_mqtt_topic = cJSON_GetObjectItem(l_mqtt, "topic");
    }
    g_config.mqtt.enable = (l_mqtt_enable != NULL) ? cJSON_IsTrue(l_mqtt_enable) : true;
    strncpy(g_config.mqtt.url, l_mqtt_url != NULL ? l_mqtt_url->valuestring : CONFIG_MQTT_URL, E12AIO_MQTT_URL_SIZE);
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
    cJSON *l_httpd_api_enable = NULL;
    if (l_httpd != NULL)
    {
        l_httpd_username = cJSON_GetObjectItem(l_httpd, "username");
        l_httpd_password = cJSON_GetObjectItem(l_httpd, "password");
        l_httpd_token = cJSON_GetObjectItem(l_httpd, "token");
        l_httpd_api_enable = cJSON_GetObjectItem(l_httpd, "enable");
    }
    strncpy(g_config.httpd.username, l_httpd_username != NULL ? l_httpd_username->valuestring : CONFIG_WEB_AUTH_USERNAME, CONFIG_WEB_AUTH_MAX_SIZE);
    strncpy(g_config.httpd.password, l_httpd_password != NULL ? l_httpd_password->valuestring : CONFIG_WEB_AUTH_PASSWORD, CONFIG_WEB_AUTH_MAX_SIZE);
    strncpy(g_config.httpd.token, l_httpd_token != NULL ? l_httpd_token->valuestring : CONFIG_WEB_TOKEN, CONFIG_WEB_AUTH_MAX_SIZE);
    g_config.httpd.api_enable = (l_httpd_api_enable != NULL) ? cJSON_IsTrue(l_httpd_api_enable) : true;

    // OTA Config
    cJSON *l_ota = cJSON_GetObjectItem(l_json, "ota");
    cJSON *l_ota_state = NULL;
    cJSON *l_ota_version = NULL;
    if (l_ota != NULL)
    {
        l_ota_state = cJSON_GetObjectItem(l_ota, "state");
        l_ota_version = cJSON_GetObjectItem(l_ota, "version");
    }
    g_config.ota.state = (l_ota_state != NULL ? l_ota_state->valueint : E12AIO_OTA_OK);
    strncpy(g_config.ota.version, l_ota_version != NULL ? l_ota_version->valuestring : E12AIO_VERSION, E12AIO_OTA_VERSION_SIZE);
    ESP_LOGI(TAG, "Config loaded!");
    cJSON_Delete(l_json);
}

/**
 * Save configuration in SPIFFS
 */
void e12aio_config_save()
{
    char l_buffer[CONFIG_JSON_BUFFER_SIZE];
    size_t l_len = e12aio_config_save_buffer_adv(g_config, (char *)l_buffer, CONFIG_JSON_BUFFER_SIZE);
    e12aio_spiffs_write(g_config_file, l_buffer, l_len);
    e12aio_spiffs_write(g_config_file, NULL, 0);
    xEventGroupClearBits(g_eventGroup, E12AIO_CONFIG_DELAYED_SAVE);
    ESP_LOGI(TAG, "All configurations was saved!");
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
    cJSON *wifi_sta_ssid = cJSON_CreateString(data.wifi.sta_ssid);
    cJSON *wifi_sta_password = cJSON_CreateString(data.wifi.sta_password);
    cJSON *wifi_ap_password = cJSON_CreateString(data.wifi.ap_password);
    cJSON *wifi_dhcp = cJSON_CreateBool(data.wifi.dhcp);
    cJSON *wifi_ip = cJSON_CreateString(data.wifi.ip);
    cJSON *wifi_netmask = cJSON_CreateString(data.wifi.netmask);
    cJSON *wifi_gateway = cJSON_CreateString(data.wifi.gateway);
    cJSON *wifi_dns1 = cJSON_CreateString(data.wifi.dns1);
    cJSON *wifi_dns2 = cJSON_CreateString(data.wifi.dns2);
    cJSON_AddItemToObject(wifi, "ap_password", wifi_ap_password);
    cJSON_AddItemToObject(wifi, "sta_ssid", wifi_sta_ssid);
    cJSON_AddItemToObject(wifi, "sta_password", wifi_sta_password);
    cJSON_AddItemToObject(wifi, "dhcp", wifi_dhcp);
    cJSON_AddItemToObject(wifi, "ip", wifi_ip);
    cJSON_AddItemToObject(wifi, "netmask", wifi_netmask);
    cJSON_AddItemToObject(wifi, "gateway", wifi_gateway);
    cJSON_AddItemToObject(wifi, "dns1", wifi_dns1);
    cJSON_AddItemToObject(wifi, "dns2", wifi_dns2);
    cJSON_AddItemToObject(json, "wifi", wifi);

    cJSON *mqtt = cJSON_CreateObject();
    cJSON *mqtt_enable = cJSON_CreateBool(data.mqtt.enable);
    cJSON *mqtt_url = cJSON_CreateString(data.mqtt.url);
    cJSON *mqtt_topic = cJSON_CreateString(data.mqtt.topic);
    cJSON_AddItemToObject(mqtt, "enable", mqtt_enable);
    cJSON_AddItemToObject(mqtt, "url", mqtt_url);
    cJSON_AddItemToObject(mqtt, "topic", mqtt_topic);
    cJSON_AddItemToObject(json, "mqtt", mqtt);

    cJSON *relay = cJSON_CreateArray();
    cJSON *relay_port1 = cJSON_CreateBool(g_config.relay.port1);
    cJSON *relay_port2 = cJSON_CreateBool(g_config.relay.port2);
    cJSON *relay_port3 = cJSON_CreateBool(g_config.relay.port3);
    cJSON_AddItemToArray(relay, relay_port1);
    cJSON_AddItemToArray(relay, relay_port2);
    cJSON_AddItemToArray(relay, relay_port3);
    cJSON_AddItemToObject(json, "relay", relay);

    cJSON *httpd = cJSON_CreateObject();
    cJSON *httpd_username = cJSON_CreateString(data.httpd.username);
    cJSON *httpd_password = cJSON_CreateString(data.httpd.password);
    cJSON *httpd_api_enable = cJSON_CreateBool(data.httpd.api_enable);
    cJSON *httpd_token = cJSON_CreateString(data.httpd.token);
    cJSON_AddItemToObject(httpd, "username", httpd_username);
    cJSON_AddItemToObject(httpd, "password", httpd_password);
    cJSON_AddItemToObject(httpd, "api_enable", httpd_api_enable);
    cJSON_AddItemToObject(httpd, "token", httpd_token);
    cJSON_AddItemToObject(json, "httpd", httpd);

    cJSON *l_ota = cJSON_CreateObject();
    cJSON *l_ota_state = cJSON_CreateNumber(g_config.ota.state);
    cJSON *l_ota_version = cJSON_CreateString(g_config.ota.version);
    cJSON_AddItemToObject(l_ota, "state", l_ota_state);
    cJSON_AddItemToObject(l_ota, "version", l_ota_version);
    cJSON_AddItemToObject(json, "ota", l_ota);

    cJSON_PrintPreallocated(json, buffer, sz, false);
    cJSON_Delete(json);
    return strlen(buffer);
}

void e12aio_config_dump()
{
    ESP_LOGD(TAG, "WIFI - AP - Password: %s", g_config.wifi.ap_password);
    ESP_LOGD(TAG, "WIFI - STA - SSID: %s", g_config.wifi.sta_ssid);
    ESP_LOGD(TAG, "WIFI - STA - Password: %s", g_config.wifi.sta_password);
    ESP_LOGD(TAG, "WIFI - DHCP: %s", g_config.wifi.dhcp ? "Enable" : "Disable");
    ESP_LOGD(TAG, "WIFI - IP: %s", g_config.wifi.ip);
    ESP_LOGD(TAG, "WIFI - Netmask: %s", g_config.wifi.netmask);
    ESP_LOGD(TAG, "WIFI - Gateway: %s", g_config.wifi.gateway);
    ESP_LOGD(TAG, "WIFI - DNS 1: %s", g_config.wifi.dns1);
    ESP_LOGD(TAG, "WIFI - DNS 2: %s", g_config.wifi.dns2);

    ESP_LOGD(TAG, "MQTT - Enable: %s", g_config.mqtt.enable ? "Enable" : "Disable");
    ESP_LOGD(TAG, "MQTT - URL: %s", g_config.mqtt.url);
    ESP_LOGD(TAG, "MQTT - TOPIC: %s", g_config.mqtt.topic);

    ESP_LOGD(TAG, "Relay - Port 1: %s", g_config.relay.port1 ? "true" : "false");
    ESP_LOGD(TAG, "Relay - Port 2: %s", g_config.relay.port2 ? "true" : "false");
    ESP_LOGD(TAG, "Relay - Port 3: %s", g_config.relay.port3 ? "true" : "false");

    ESP_LOGD(TAG, "HTTPD - Username: %s", g_config.httpd.username);
    ESP_LOGD(TAG, "HTTPD - Password: %s", g_config.httpd.password);
    ESP_LOGD(TAG, "HTTPD - API: %s", g_config.httpd.api_enable ? "Enable" : "Disable");
    ESP_LOGD(TAG, "HTTPD - Token: %s", g_config.httpd.token);

    ESP_LOGD(TAG, "OTA - State: %d", g_config.ota.state);
    ESP_LOGD(TAG, "OTA - Version: %s", g_config.ota.version);
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
        l_port = &(g_config.relay.port1);
        break;
    case 2:
        l_port = &(g_config.relay.port2);
        break;
    case 3:
        l_port = &(g_config.relay.port3);
        break;
    }
    return l_port;
}

void e12aio_config_relay_set(uint8_t relay, bool status)
{
    e12aio_config_relay_set_adv(relay, status, true);
}

/**
 * Relay set advanced with a little more parameters.
 */
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