#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "mqtt.h"

static const char *TAG = "mqtt.c";

#define MQTT_CONNECTED BIT0

static EventGroupHandle_t mqtt_event_group;

void mqtt_init()
{
    mqtt_event_group = xEventGroupCreate();
    xTaskCreate(mqtt_loop_task, "mqtt_loop", 2048, NULL, 5, NULL);
    xTaskCreate(mqtt_keepalive_task, "mqtt_keepalive", 2048, NULL, 2, NULL);
}

void mqtt_loop_task(void *arg)
{
    for (;;)
    {
        ESP_LOGI(TAG, "mqtt status checking...");
        vTaskDelay((60 * 1000) / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void mqtt_keepalive_task(void *arg)
{
    const uint16_t delay = (CONFIG_MQTT_KEEPALIVE_TIMEOUT * 1000) / portTICK_PERIOD_MS;
    for (;;)
    {
        ESP_LOGI(TAG, "Sending mqtt keep alive...");
        vTaskDelay(delay);
    }
    vTaskDelete(NULL);
}