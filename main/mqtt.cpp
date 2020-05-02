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
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "string"
#include "esp_log.h"
#include "esp_timer.h"
#include "config.hpp"
#include "wifi.hpp"
#include "relay.hpp"
#include "mqtt.hpp"

static const char *TAG = "mqtt.c";
static EventGroupHandle_t g_mqtt_event_group;
static char g_mqttUrl[D_TOPIC_SIZE];

/**
 * Handle MQTT Events inside the stack
 * @param event Event details
 */
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        MQTT.sendOnline();
        xEventGroupSetBits(g_mqtt_event_group, D_MQTT_CONNECTED);
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

/**
 * Setup MQTT Class
 */
MQTTClass::MQTTClass()
{
    g_mqtt_event_group = xEventGroupCreate();
}

/**
 * Main initialization, generally called in app_main after priority tasks.
 */
void MQTTClass::init()
{
    this->updateConfig();
    xTaskCreate(MQTT.loop, "mqtt_loop", 2048, NULL, 5, NULL);
}

/**
 * Routine called after any configuration update. Most of the time, it is a callback function.
 */
void MQTTClass::updateConfig()
{
    // g_mqttUrl Example: mqtt://user:pass@host:port
    snprintf(g_mqttUrl, D_TOPIC_SIZE, "%s", Config.getValues().mqtt.url.c_str());
    // g_baseTopic Example: home%s/e12aio3_CC9900/%s
    // snprintf(g_baseTopic, D_TOPIC_SIZE, "%s\%s/%s/\%s", Config.getValues().mqtt.topic.c_str(), Config.getName().c_str());

    // Rebind all subscriptions and topics
    if (MQTT.isConnected())
    {
        MQTT.disconnect();
        MQTT.connect();
    }
}

/**
 * Connect to a MQTT Server
 */
void MQTTClass::connect()
{
    ESP_LOGD(TAG, "MQTT: Connecting");
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.event_handle = mqtt_event_handler;
    mqtt_cfg.uri = g_mqttUrl;
    this->m_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(this->m_client);
}

/**
 * Disconnect from a MQTT Server
 */
void MQTTClass::disconnect()
{
    ESP_LOGD(TAG, "MQTT: Disconnecting");
    esp_mqtt_client_stop(this->m_client);
    vTaskDelay(500 / portTICK_PERIOD_MS);
}

/**
 * FreeRTOS Task to monitor WiFi Sate versus MQTT State.
 * @param arg context FreeRTOS argument
 */
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

/**
 * FreeRTOS Task: Maintain a KeepAlive to Home Assistant
 * @param arg context FreeRTOS argument 
 */
void MQTTClass::keepAlive(void *arg)
{
    const uint16_t delay = (CONFIG_MQTT_KEEPALIVE_TIMEOUT) / portTICK_PERIOD_MS;
    for (; (xEventGroupGetBits(g_mqtt_event_group) & D_MQTT_CONNECTED);)
    {
        ESP_LOGI(TAG, "MQTT: Sending keepAlive");
        MQTT.sendKeepAlive();
        vTaskDelay(delay);
    }
    vTaskDelete(NULL);
}

/**
 * Send all keepAlive topics to MQTT (used by Home Assistant).
 * Sample Topics and payloads:
 * home/sensor/e12aio3_CC9900/uptime, 00:01:03
 * home/sensor/e12aio3_CC9900/ipaddr, 10.10.10.10
 * home/sensor/e12aio3_CC9900/model, E12-AIO3
 * home/sensor/e12aio3_CC9900/build, Fri May 1 23:38:48 2020
 */
void MQTTClass::sendKeepAlive()
{
    const uint16_t l_delay = CONFIG_MQTT_DELAY_RUSH_PACKAGE / portTICK_PERIOD_MS;
    const char *topic = Config.getValues().mqtt.topic.c_str();
    const char *name = Config.getName().c_str();
    char l_topic[D_TOPIC_SIZE];
    char l_payload[CONFIG_JSON_BUFFER_SIZE];

    // uptime
    uint32_t sec = esp_timer_get_time() / 1000ULL / 1000ULL;
    uint16_t min = sec / 60;
    uint16_t hour = min / 60;
    snprintf(l_topic, D_TOPIC_SIZE, "%ssensor/%s/uptime", topic, name);
    snprintf(l_payload, CONFIG_JSON_BUFFER_SIZE, "%02d:%02d:%02d", hour, min % 60, sec % 60);
    esp_mqtt_client_publish(this->m_client, l_topic, l_payload, strlen(l_payload), 1, 0);
    vTaskDelay(l_delay);

    // ipaddr
    snprintf(l_topic, D_TOPIC_SIZE, "%ssensor/%s/ipaddr", topic, name);
    snprintf(l_payload, CONFIG_JSON_BUFFER_SIZE, "%s", WIFI.getStationIP());
    esp_mqtt_client_publish(this->m_client, l_topic, l_payload, strlen(l_payload), 1, 0);
    vTaskDelay(l_delay);

    // model
    snprintf(l_topic, D_TOPIC_SIZE, "%ssensor/%s/model", topic, name);
    snprintf(l_payload, CONFIG_JSON_BUFFER_SIZE, "E12-AIO3");
    esp_mqtt_client_publish(this->m_client, l_topic, l_payload, strlen(l_payload), 1, 0);
    vTaskDelay(l_delay);

    // build timestamp
    snprintf(l_topic, D_TOPIC_SIZE, "%ssensor/%s/build", topic, name);
    snprintf(l_payload, CONFIG_JSON_BUFFER_SIZE, "%s", __TIMESTAMP__);
    esp_mqtt_client_publish(this->m_client, l_topic, l_payload, strlen(l_payload), 1, 0);
    vTaskDelay(l_delay);
}

/**
 * Send a switch configuration to home assistant, based on each relay.
 * Sample Topic: home/switch/e12aio3_CC9900/relay1/config, payload:
 * { 
 *   "~": "home/switch/e12aio3_CC9900/relay1", 
 *   "cmd_t": "~/set", 
 *   "stat_t": "~", 
 *   "name": "e12aio3_CC9900_relay1" 
 * }
 * @param relay relay number from 1 to 3
 */
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

/**
 * Send relay status to mqtt
 * Sample topic: home/switch/e12aio3_CC9900/relay1, payload: ON
 * @param relay relay number from 1 to 3
 */
void MQTTClass::sendRelayStatus(uint8_t relay)
{
    const char *topic = Config.getValues().mqtt.topic.c_str();
    const char *name = Config.getName().c_str();
    std::string l_payload = Relay.getStatus(relay) ? "ON" : "OFF";
    char l_topic[D_TOPIC_SIZE];
    snprintf(l_topic, D_TOPIC_SIZE, "%sswitch/%s/relay%d", topic, name, relay);
    esp_mqtt_client_publish(this->m_client, l_topic, l_payload.c_str(), l_payload.length(), 1, 0);
}

/**
 * Subscribe in each relay topics
 * Sample topic: home/switch/e12aio3_CC9900/relay1/set
 * 
 * @param relay relay number from 1 to 3
 */
void MQTTClass::subscribeRelay(uint8_t relay)
{
    char l_topic[D_TOPIC_SIZE];
    this->buildRelaySetTopic(relay, l_topic, D_TOPIC_SIZE);
    esp_mqtt_client_subscribe(this->m_client, l_topic, 1);
}

/**
 * Generate a `set topic` for each relay
 * Sample topic: home/switch/e12aio3_CC9900/relay1/set
 * 
 * @param relay relay number from 1 to 3
 * @param buffer a char[] buffer to receive the result
 * @param sz a char[] size
 * @return length of buffer
 */
size_t MQTTClass::buildRelaySetTopic(uint8_t relay, char *buffer, size_t sz)
{
    const char *topic = Config.getValues().mqtt.topic.c_str();
    const char *name = Config.getName().c_str();
    return snprintf(buffer, sz, "%sswitch/%s/relay%d/set", topic, name, relay);
}

/**
 * Send a configuration for sensors to home assistant.
 * Sample topics and payloads:
 * 
 * home/sensor/e12aio3_CC9900/uptime/config
 * { "name": "e12aio3_CC9900_uptime", "stat_t": "home/sensor/e12aio3_CC9900/uptime" }
 * 
 * home/sensor/e12aio3_CC9900/ipaddr/config
 * { "name": "e12aio3_CC9900_ipaddr", "stat_t": "home/sensor/e12aio3_CC9900/ipaddr" }
 * 
 * home/sensor/e12aio3_CC9900/model/config
 * { "name": "e12aio3_CC9900_model", "stat_t": "home/sensor/e12aio3_CC9900/model" }
 * 
 * home/sensor/e12aio3_CC9900/build/config
 * { "name": "e12aio3_CC9900_build", "stat_t": "home/sensor/e12aio3_CC9900/build" }
 * 
 * home/sensor/e12aio3_CC9900/freemem/config
 * { "name": "e12aio3_CC9900_freemem", "stat_t": "home/sensor/e12aio3_CC9900/freemem" }
 * 
 */
void MQTTClass::hassConfigSensors()
{
    const char *topic = Config.getValues().mqtt.topic.c_str();
    const char *name = Config.getName().c_str();
    char l_topic[D_TOPIC_SIZE];
    char l_payload[CONFIG_JSON_BUFFER_SIZE];
    const uint16_t l_delay = CONFIG_MQTT_DELAY_RUSH_PACKAGE / portTICK_PERIOD_MS;

    // uptime
    snprintf(l_topic, D_TOPIC_SIZE, "%ssensor/%s/uptime/config", topic, name);
    snprintf(l_payload, CONFIG_JSON_BUFFER_SIZE, "{ \"name\": \"%s_uptime\", \"stat_t\": \"%ssensor/%s/uptime\" }", name, topic, name);
    esp_mqtt_client_publish(this->m_client, l_topic, l_payload, strlen(l_payload), 1, 0);
    vTaskDelay(l_delay);

    // ipaddr
    snprintf(l_topic, D_TOPIC_SIZE, "%ssensor/%s/ipaddr/config", topic, name);
    snprintf(l_payload, CONFIG_JSON_BUFFER_SIZE, "{ \"name\": \"%s_ipaddr\", \"stat_t\": \"%ssensor/%s/ipaddr\" }", name, topic, name);
    esp_mqtt_client_publish(this->m_client, l_topic, l_payload, strlen(l_payload), 1, 0);
    vTaskDelay(l_delay);

    // model
    snprintf(l_topic, D_TOPIC_SIZE, "%ssensor/%s/model/config", topic, name);
    snprintf(l_payload, CONFIG_JSON_BUFFER_SIZE, "{ \"name\": \"%s_model\", \"stat_t\": \"%ssensor/%s/model\" }", name, topic, name);
    esp_mqtt_client_publish(this->m_client, l_topic, l_payload, strlen(l_payload), 1, 0);
    vTaskDelay(l_delay);

    // build timestamp
    snprintf(l_topic, D_TOPIC_SIZE, "%ssensor/%s/build/config", topic, name);
    snprintf(l_payload, CONFIG_JSON_BUFFER_SIZE, "{ \"name\": \"%s_build\", \"stat_t\": \"%ssensor/%s/build\" }", name, topic, name);
    esp_mqtt_client_publish(this->m_client, l_topic, l_payload, strlen(l_payload), 1, 0);
    vTaskDelay(l_delay);

    // free mem
    snprintf(l_topic, D_TOPIC_SIZE, "%ssensor/%s/freemem/config", topic, name);
    snprintf(l_payload, CONFIG_JSON_BUFFER_SIZE, "{ \"name\": \"%s_freemem\", \"stat_t\": \"%ssensor/%s/freemem\" }", name, topic, name);
    esp_mqtt_client_publish(this->m_client, l_topic, l_payload, strlen(l_payload), 1, 0);
    vTaskDelay(l_delay);
}

/**
 * Function called when connect to a MQTT Server, for send many initial informations:
 * 1. Configure Sensors
 * 2. Subscribe in each relay topic
 * 3. Configure in home assistant each relay topic
 * 4. Send to mqtt (home assistant) each relay status
 * 5. Send keepAlive: uptime, ipaddr, build, model
 */
void MQTTClass::sendOnline()
{
    const uint16_t l_delay = CONFIG_MQTT_DELAY_RUSH_PACKAGE / portTICK_PERIOD_MS;
    this->hassConfigSensors();
    for (uint8_t l_x = 1; l_x < 4; l_x++)
    {
        this->subscribeRelay(l_x);
        this->hassConfigRelay(l_x);
        this->sendRelayStatus(l_x);
        vTaskDelay(l_delay);
    }
    this->subscribeActions();
    this->sendKeepAlive();
}

/**
 * Check if it is connected to a MQTT Server
 * @return connected?
 */
bool MQTTClass::isConnected()
{
    return xEventGroupGetBits(g_mqtt_event_group) & D_MQTT_CONNECTED;
}

/**
 * Process any received message from MQTT Server
 * 
 * Sample topics, payloads:
 * home/switch/e12aio3_CC9900/relay1/set, ON
 * home/switch/e12aio3_CC9900/relay2/set, ON
 * home/switch/e12aio3_CC9900/relay3/set, ON
 * home/action/e12aio3_CC9900/config/set, { ...config json...}
 * home/action/e12aio3_CC9900/config/get, json
 * home/action/e12aio3_CC9900/scan/get, json
 * home/action/e12aio3_CC9900/restart/set, yes
 * 
 * @param topic subscribed topic who received the message payload
 * @param payload payload message buffer
 */
void MQTTClass::received(const char *topic, const char *payload)
{
    ESP_LOGD(TAG, "Topic: %s, payload [%s]", topic, payload);
    std::string l_relayTopics = Config.getValues().mqtt.topic + "switch/" + Config.getName() + "/relay";
    std::string l_actionConfig = Config.getValues().mqtt.topic + "action/" + Config.getName() + "/config";
    std::string l_actionScan = Config.getValues().mqtt.topic + "action/" + Config.getName() + "/scan/get";
    std::string l_actionRestart = Config.getValues().mqtt.topic + "action/" + Config.getName() + "/restart/set";

    if (strncmp(topic, l_relayTopics.c_str(), l_relayTopics.length()) == 0)
    {
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
    else if (strncmp(topic, l_actionConfig.c_str(), l_actionConfig.length()) == 0)
    {
        std::string l_actionConfig_set = l_actionConfig + "/set";
        if (strncmp(topic, l_actionConfig_set.c_str(), l_actionConfig_set.length()) == 0)
        {
            // set config
            Config.loadInMemory(payload);
            Config.lazySave();
        }
        else
        {
            // get config
            char buffer[CONFIG_JSON_BUFFER_SIZE];
            Config.saveInMemory(buffer, CONFIG_JSON_BUFFER_SIZE);
            esp_mqtt_client_publish(this->m_client, l_actionConfig.c_str(), buffer, strlen(buffer), 1, 0);
        }
    }
    else if (strncmp(topic, l_actionScan.c_str(), l_actionScan.length()) == 0)
    {
        // scan networks and send
        // TODO: needs to be implement
    }
    else if (strncmp(topic, l_actionRestart.c_str(), l_actionRestart.length()) == 0)
    {
        // restart
        esp_restart();
    }
}

void MQTTClass::subscribeActions()
{
    const uint16_t l_delay = CONFIG_MQTT_DELAY_RUSH_PACKAGE / portTICK_PERIOD_MS;
    std::string l_actionConfig = Config.getValues().mqtt.topic + "action/" + Config.getName() + "/config";
    std::string l_actionScan = Config.getValues().mqtt.topic + "action/" + Config.getName() + "/scan/get";
    std::string l_actionRestart = Config.getValues().mqtt.topic + "action/" + Config.getName() + "/restart/set";
    esp_mqtt_client_subscribe(this->m_client, (l_actionConfig + "/set").c_str(), 1);
    vTaskDelay(l_delay);
    esp_mqtt_client_subscribe(this->m_client, (l_actionConfig + "/get").c_str(), 1);
    vTaskDelay(l_delay);
    esp_mqtt_client_subscribe(this->m_client, l_actionScan.c_str(), 1);
    vTaskDelay(l_delay);
    esp_mqtt_client_subscribe(this->m_client, l_actionRestart.c_str(), 1);
    vTaskDelay(l_delay);
}

MQTTClass MQTT;