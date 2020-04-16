#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "config.h"

const char *TAG = "config.c";

#define CONFIG_LOADED BIT0
#define CONFIG_SAVED BIT1

static EventGroupHandle_t config_event_group;

void config_init()
{
    ESP_LOGI(TAG, "Initializing configuration...");
    config_event_group = xEventGroupCreate();
}