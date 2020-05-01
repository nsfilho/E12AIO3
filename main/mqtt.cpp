#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "mqtt.hpp"

static const char *TAG = "mqtt.c";

#define MQTT_CONNECTED BIT0

static EventGroupHandle_t mqtt_event_group;

void MQTTClass::init()
{
    mqtt_event_group = xEventGroupCreate();
    xTaskCreate(this->loop, "mqtt_loop", 2048, NULL, 5, NULL);
    xTaskCreate(this->keepAlive, "mqtt_keepalive", 2048, NULL, 2, NULL);
}

void MQTTClass::connect()
{
    ESP_LOGD(TAG, "MQTT: Connecting");
}

void MQTTClass::disconnect()
{
    ESP_LOGD(TAG, "MQTT: Disconnecting");
}

void MQTTClass::loop(void *arg)
{
    for (;;)
    {
        ESP_LOGD(TAG, "MQTT: loop");
        vTaskDelay((60 * 1000) / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void MQTTClass::keepAlive(void *arg)
{
    const uint16_t delay = (CONFIG_MQTT_KEEPALIVE_TIMEOUT * 1000) / portTICK_PERIOD_MS;
    for (;;)
    {
        ESP_LOGI(TAG, "MQTT: Sending keepAlive");
        vTaskDelay(delay);
    }
    vTaskDelete(NULL);
}

MQTTClass MQTT;