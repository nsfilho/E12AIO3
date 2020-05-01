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
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 15,
        .format_if_mount_failed = true,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }
}

/**
 * Prepare your configuration or set default values
 */
void ConfigClass::prepare_configs()
{
    struct stat st;
    if (stat(g_config_file, &st) == 0)
    {
        // loading from system
        this->load();
    }
    else
    {
        // Initialize the configuration with fabric defaults
        g_config.wifi.ssid = CONFIG_WIFI_SSID;
        g_config.wifi.password = CONFIG_WIFI_PASSWORD;
        g_config.mqtt.url = CONFIG_MQTT_URL;
        g_config.mqtt.topic = std::string(CONFIG_MQTT_TOPIC_BASE) + Config.getName();
        g_config.relay.port1 = false,
        g_config.relay.port2 = false,
        g_config.relay.port3 = false,
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
    FILE *fp = fopen(g_config_file, "r");
    if (fp == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file: %s for reading", g_config_file);
        return;
    }
    fread(&l_buffer, 1, CONFIG_JSON_BUFFER_SIZE, fp);
    fclose(fp);
    this->loadInMemory(l_buffer);
}

void ConfigClass::loadInMemory(const char *buffer)
{
    // parse file
    cJSON *json = cJSON_Parse(buffer);
    cJSON *wifi = cJSON_GetObjectItem(json, "wifi");
    if (wifi != NULL)
    {
        cJSON *wifi_ssid = cJSON_GetObjectItem(wifi, "ssid");
        cJSON *wifi_password = cJSON_GetObjectItem(wifi, "password");
        g_config.wifi.ssid = wifi_ssid != NULL ? wifi_ssid->valuestring : CONFIG_WIFI_SSID;
        g_config.wifi.password = wifi_password != NULL ? wifi_password->valuestring : CONFIG_WIFI_PASSWORD;
    }
    cJSON *mqtt = cJSON_GetObjectItem(json, "mqtt");
    if (mqtt != NULL)
    {
        cJSON *mqtt_url = cJSON_GetObjectItem(mqtt, "url");
        cJSON *mqtt_topic = cJSON_GetObjectItem(mqtt, "topic");
        g_config.mqtt.url = mqtt_url != NULL ? mqtt_url->valuestring : CONFIG_MQTT_URL;
        g_config.mqtt.topic = mqtt_topic != NULL ? mqtt_topic->valuestring : (std::string(CONFIG_MQTT_TOPIC_BASE) + Config.getName()).c_str();
    }
    cJSON *relay = cJSON_GetObjectItem(json, "relay");
    if (relay != NULL)
    {
        cJSON *status = NULL;
        uint8_t relay_counter = 1;
        cJSON_ArrayForEach(status, relay)
        {
            switch (relay_counter)
            {
            case 1:
                g_config.relay.port1 = cJSON_IsTrue(status);
                break;
            case 2:
                g_config.relay.port2 = cJSON_IsTrue(status);
                break;
            case 3:
                g_config.relay.port3 = cJSON_IsTrue(status);
                break;
            }
            relay_counter++;
        }
    }
    ESP_LOGI(TAG, "Config loaded!");
    cJSON_Delete(json);
}

/**
 * Save configuration in SPIFFS
 */
void ConfigClass::save()
{
    char buffer[CONFIG_JSON_BUFFER_SIZE];
    FILE *fp = fopen(g_config_file, "w+");
    if (fp == NULL)
    {
        ESP_LOGE(TAG, "Failed to write file: %s", g_config_file);
        return;
    }
    this->saveInMemory(g_config, buffer, CONFIG_JSON_BUFFER_SIZE);
    fprintf(fp, "%s", buffer);
    fclose(fp);
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
    const uint16_t size = snprintf(buffer, sz, "%s", cJSON_Print(json));
    cJSON_Delete(json);
    return size;
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

ConfigClass Config;