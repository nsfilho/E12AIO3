#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "string"
#include "esp_log.h"
#include "config.hpp"
#include "wifi.hpp"
#include "relay.hpp"
#include "mqtt.hpp"

static const char *TAG = "mqtt.c";
static EventGroupHandle_t g_mqtt_event_group;
static char g_mqtt_url[D_TOPIC_SIZE];

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        xEventGroupSetBits(g_mqtt_event_group, D_MQTT_CONNECTED);
        MQTT.sendOnline();
        xTaskCreate(MQTT.keepAlive, "mqtt_keepalive", 2048, NULL, 2, NULL);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        xEventGroupClearBits(g_mqtt_event_group, D_MQTT_CONNECTED);
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        {
            const uint8_t topic_len = event->topic_len;
            const uint8_t payload_len = event->data_len;
            char topic[topic_len + 1];
            char payload[payload_len + 1];
            memset(topic, 0, topic_len);
            memset(payload, 0, payload_len);
            strncpy(topic, event->topic, topic_len);
            strncpy(payload, event->data, payload_len);
            MQTT.received(topic, payload);
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    }
    return ESP_OK;
}

MQTTClass::MQTTClass()
{
    g_mqtt_event_group = xEventGroupCreate();
}

void MQTTClass::init()
{
    xTaskCreate(MQTT.loop, "mqtt_loop", 2048, NULL, 5, NULL);
}

void MQTTClass::connect()
{
    ESP_LOGD(TAG, "MQTT: Connecting");
    snprintf(g_mqtt_url, D_TOPIC_SIZE, "%s", Config.getValues().mqtt.url.c_str());
    esp_mqtt_client_config_t mqtt_cfg = {
        .event_handle = mqtt_event_handler,
        .host = NULL,
        .uri = g_mqtt_url,
    };
    this->m_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(this->m_client);
}

void MQTTClass::disconnect()
{
    ESP_LOGD(TAG, "MQTT: Disconnecting");
    esp_mqtt_client_stop(this->m_client);
}

void MQTTClass::loop(void *arg)
{
    const uint16_t delay = CONFIG_MQTT_RETRY_TIMEOUT / portTICK_PERIOD_MS;
    for (;;)
    {
        ESP_LOGD(TAG, "MQTT: loop");
        if (WIFI.isConnected() && !MQTT.isConnected())
            MQTT.connect();
        else if (!WIFI.isConnected() && MQTT.isConnected())
            MQTT.disconnect();
        vTaskDelay(delay);
    }
    vTaskDelete(NULL);
}

void MQTTClass::keepAlive(void *arg)
{
    const uint16_t delay = (CONFIG_MQTT_KEEPALIVE_TIMEOUT) / portTICK_PERIOD_MS;
    for (; (xEventGroupGetBits(g_mqtt_event_group) & D_MQTT_CONNECTED);)
    {
        ESP_LOGI(TAG, "MQTT: Sending keepAlive");
        vTaskDelay(delay);
    }
    vTaskDelete(NULL);
}

void MQTTClass::hassConfigRelay(uint8_t relay)
{
    // present as switch (relay1, relay2, relay3)
    const char *topic = Config.getValues().mqtt.topic.c_str();
    const char *name = Config.getName().c_str();
    char l_topic[D_TOPIC_SIZE];
    char l_payload[CONFIG_JSON_BUFFER_SIZE];
    snprintf(l_topic, D_TOPIC_SIZE, "%sswitch/%s/relay%d/config", topic, name, relay);
    snprintf(l_payload, CONFIG_JSON_BUFFER_SIZE, "{ \"~\": \"%sswitch/%s/relay%d\", \"cmd_t\": \"~/set\", \"stat_t\": \"~\", \"name\": \"%s_relay_%d\" }",
             topic, name, relay, name, relay);
    esp_mqtt_client_publish(this->m_client, l_topic, l_payload, strlen(l_payload), 1, 0);
}

void MQTTClass::sendRelayStatus(uint8_t relay)
{
    const char *topic = Config.getValues().mqtt.topic.c_str();
    const char *name = Config.getName().c_str();
    std::string l_payload = Relay.getStatus(relay) ? "ON" : "OFF";
    char l_topic[D_TOPIC_SIZE];
    snprintf(l_topic, D_TOPIC_SIZE, "%sswitch/%s/relay%d", topic, name, relay);
    esp_mqtt_client_publish(this->m_client, l_topic, l_payload.c_str(), l_payload.length(), 1, 0);
}

void MQTTClass::subscribeRelay(uint8_t relay)
{
    char l_topic[D_TOPIC_SIZE];
    this->buildRelaySetTopic(relay, l_topic, D_TOPIC_SIZE);
    esp_mqtt_client_subscribe(this->m_client, l_topic, 1);
}

size_t MQTTClass::buildRelaySetTopic(uint8_t relay, char *buffer, size_t sz)
{
    const char *topic = Config.getValues().mqtt.topic.c_str();
    const char *name = Config.getName().c_str();
    return snprintf(buffer, sz, "%sswitch/%s/relay%d/set", topic, name, relay);
}

void MQTTClass::sendOnline()
{
    for (uint8_t l_x = 1; l_x < 4; l_x++)
    {
        this->subscribeRelay(l_x);
        this->hassConfigRelay(l_x);
        this->sendRelayStatus(l_x);
    }
}

bool MQTTClass::isConnected()
{
    return xEventGroupGetBits(g_mqtt_event_group) & D_MQTT_CONNECTED;
}

void MQTTClass::received(const char *topic, const char *payload)
{
    ESP_LOGD(TAG, "Topic: %s, payload [%s]", topic, payload);
    char l_topic[D_TOPIC_SIZE];
    for (uint8_t l_x = 1; l_x < 4; l_x++)
    {
        this->buildRelaySetTopic(l_x, l_topic, D_TOPIC_SIZE);
        if (strncmp(topic, l_topic, strlen(l_topic)) == 0)
        {
            bool l_status = strcmp(payload, "ON") == 0;
            Relay.setStatus(l_x, l_status);
            this->sendRelayStatus(l_x);
        }
    }
}

MQTTClass MQTT;