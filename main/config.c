#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "config.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "cJSON.h"
#include "sdkconfig.h"
#include "string.h"

const char *TAG = "config.c";
const char *g_config_file = "/spiffs/config.json";
static char g_esp_name[33];

static config_t g_config;
static EventGroupHandle_t g_config_event_group;

void config_init_task(void *args)
{
    config_esp_name();
    config_prepare_spiffs();
    config_prepare_configs();
    xEventGroupSetBits(g_config_event_group, D_CONFIG_LOADED);
    ESP_LOGD(TAG, "Config was loaded!");
    vTaskDelete(NULL);
}

/**
 * Initialize all configurations
 */
void config_init()
{
    ESP_LOGI(TAG, "Initializing configuration sub-system...");
    // Prepara as informações sobre o ESP que está rodando
    g_config_event_group = xEventGroupCreate();
    xTaskCreate(config_init_task, "config_init", 2048, NULL, configMAX_PRIORITIES - 1, NULL);
}

/**
 * Generate g_esp_name
 */
void config_esp_name()
{
    uint8_t l_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, l_mac);
    snprintf(g_esp_name, sizeof(g_esp_name), "e12aio_%02X%02X%02X", l_mac[3], l_mac[4], l_mac[5]);
    ESP_LOGI(TAG, "Name: %s", g_esp_name);
}

/**
 * Prepare spiffs structure do save and load data
 */
void config_prepare_spiffs()
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
void config_prepare_configs()
{
    struct stat st;
    if (stat(g_config_file, &st) == 0)
    {
        // loading from system
        config_load();
    }
    else
    {
        // Initialize the configuration with fabric defaults
        strcpy(g_config.wifi_ssid, CONFIG_WIFI_SSID);
        strcpy(g_config.wifi_password, CONFIG_WIFI_PASSWORD);
        strcpy(g_config.mqtt_url, CONFIG_MQTT_URL);
        g_config.relay1 = false,
        g_config.relay2 = false,
        g_config.relay3 = false,
        config_lazy_save();
    }
}

/**
 * Save all your configurations in a lazy mode (this is a auxiliary task).
 */
void config_lazy_save_task(void *args)
{
    vTaskDelay(CONFIG_SAVE_LAZY_TIMEOUT / portTICK_PERIOD_MS);
    config_save();
    xEventGroupClearBits(g_config_event_group, D_CONFIG_DELAYED_SAVE);
    ESP_LOGD(TAG, "Config saved (lazy)!");
    vTaskDelete(NULL);
}

/**
 * Save all your configurations in a lazy mode (avoid to many simultaenous write)
 */
void config_lazy_save()
{
    if (!(xEventGroupGetBits(g_config_event_group) & D_CONFIG_DELAYED_SAVE))
    {
        ESP_LOGD(TAG, "Creating a lazy saving task");
        xEventGroupSetBits(g_config_event_group, D_CONFIG_DELAYED_SAVE);
        xTaskCreate(config_lazy_save_task, "config_lazy_save", 2048, NULL, 1, NULL);
    }
    else
    {
        ESP_LOGD(TAG, "A lazy saving task already started!");
    }
}

void config_load()
{
    // Config file exists
    ESP_LOGI(TAG, "File: /spiffs/config.json");
    char l_buffer[D_CONFIG_BUFFER_SIZE];
    FILE *fp = fopen(g_config_file, "r");
    if (fp == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file: %s for reading", g_config_file);
        return;
    }
    fread(&l_buffer, 1, D_CONFIG_BUFFER_SIZE, fp);
    fclose(fp);
    config_load_inMemory(l_buffer);
}

void config_load_inMemory(const char *buffer)
{
    // parse file
    cJSON *json = cJSON_Parse(buffer);
    cJSON *wifi = cJSON_GetObjectItem(json, "wifi");
    if (wifi != NULL)
    {
        cJSON *wifi_ssid = cJSON_GetObjectItem(wifi, "ssid");
        cJSON *wifi_password = cJSON_GetObjectItem(wifi, "password");
        strcpy(g_config.wifi_ssid, wifi_ssid != NULL ? wifi_ssid->valuestring : CONFIG_WIFI_SSID);
        strcpy(g_config.wifi_password, wifi_password != NULL ? wifi_password->valuestring : CONFIG_WIFI_PASSWORD);
    }
    cJSON *mqtt = cJSON_GetObjectItem(json, "mqtt");
    if (mqtt != NULL)
    {
        cJSON *mqtt_url = cJSON_GetObjectItem(mqtt, "url");
        strcpy(g_config.mqtt_url, mqtt_url != NULL ? mqtt_url->valuestring : CONFIG_MQTT_URL);
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
                g_config.relay1 = cJSON_IsTrue(status);
                break;
            case 2:
                g_config.relay2 = cJSON_IsTrue(status);
                break;
            case 3:
                g_config.relay3 = cJSON_IsTrue(status);
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
void config_save()
{
    char buffer[D_CONFIG_BUFFER_SIZE];
    FILE *fp = fopen(g_config_file, "w+");
    if (fp == NULL)
    {
        ESP_LOGE(TAG, "Failed to write file: %s", g_config_file);
        return;
    }
    config_save_inMemory(g_config, buffer, D_CONFIG_BUFFER_SIZE);
    fprintf(fp, "%s", buffer);
    fclose(fp);
}

size_t config_save_inMemory(config_t data, char *buffer, size_t sz)
{

    // Generate a clear config.json
    cJSON *json = cJSON_CreateObject();
    cJSON *wifi = cJSON_CreateObject();
    cJSON *wifi_ssid = cJSON_CreateString(data.wifi_ssid);
    cJSON *wifi_password = cJSON_CreateString(data.wifi_password);
    cJSON_AddItemToObject(wifi, "ssid", wifi_ssid);
    cJSON_AddItemToObject(wifi, "password", wifi_password);
    cJSON_AddItemToObject(json, "wifi", wifi);
    cJSON *mqtt = cJSON_CreateObject();
    cJSON *mqtt_url = cJSON_CreateString(data.mqtt_url);
    cJSON_AddItemToObject(mqtt, "url", mqtt_url);
    cJSON_AddItemToObject(json, "mqtt", mqtt);
    snprintf(buffer, sz, "%s", cJSON_Print(json));
    cJSON_Delete(json);
    return strlen(buffer);
}

void config_dump()
{
    ESP_LOGD(TAG, "WIFI - SSID: %s", g_config.wifi_ssid);
    ESP_LOGD(TAG, "WIFI - Password: %s", g_config.wifi_password);
    ESP_LOGD(TAG, "MQTT - URL: %s", g_config.mqtt_url);
    ESP_LOGD(TAG, "Relay - Port 1: %s", g_config.relay1 ? "true" : "false");
    ESP_LOGD(TAG, "Relay - Port 2: %s", g_config.relay2 ? "true" : "false");
    ESP_LOGD(TAG, "Relay - Port 3: %s", g_config.relay3 ? "true" : "false");
}

void config_wait_loaded()
{
    xEventGroupWaitBits(g_config_event_group, D_CONFIG_LOADED, pdFALSE, pdTRUE, portMAX_DELAY);
}