#include <sys/stat.h>
#include "config.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "cJSON.h"

const char *TAG = "config.c";
static char ESP_NAME[33];

const char *config_file = "/spiffs/config.json";

void config_init()
{
    ESP_LOGI(TAG, "Initializing configuration...");
    // Prepara as informações sobre o ESP que está rodando
    config_event_group = xEventGroupCreate();
    config_name();
    config_load();
    xEventGroupSetBits(config_event_group, CONFIG_LOADED);
}

void config_name()
{
    uint8_t l_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, l_mac);
    snprintf(ESP_NAME, sizeof(ESP_NAME), "e12aio_%02X%02X%02X", l_mac[3], l_mac[4], l_mac[5]);
    ESP_LOGI(TAG, "Name: %s", ESP_NAME);
}

void config_load()
{
    ESP_LOGI(TAG, "Initializing SPIFFS");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 15,
        .format_if_mount_failed = true,
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
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
    struct stat st;
    if (stat(config_file, &st) == 0)
    {
        // Config file exists
        ESP_LOGI(TAG, "File: /spiffs/config.json, size: %li", st.st_size);
        char *l_buffer = malloc(st.st_size + 1);
        FILE *fp = fopen(config_file, "r");
        if (fp == NULL)
        {
            ESP_LOGE(TAG, "Failed to open file: %s for reading", config_file);
            return;
        }
        fread(l_buffer, st.st_size, st.st_size, fp);
        fclose(fp);

        // parse file
        cJSON *json = cJSON_Parse(l_buffer);

        free(l_buffer);
    }
    else
    {
        // Generate a clear config.json
    }
}