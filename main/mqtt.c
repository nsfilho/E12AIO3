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
/**
 * 2020-07-19 - Big change in this routines. Some examples:
 * - changed the way to process messages (continuous via queue)
 * - changed the way to solve disconnects (future)
 * - implementing queue to send (for control delay in a unique point) (future)
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>
#include <esp_system.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <mqtt_client.h>
#include "e12aio.h"
#include "config.h"
#include "wifi.h"
#include "relay.h"
#include "mqtt.h"
#include "utils.h"

static const char *TAG = "mqtt.c";

#ifdef CONFIG_COMPONENT_MQTT
static const uint16_t g_delay = CONFIG_MQTT_DELAY_RUSH_PACKAGE / portTICK_PERIOD_MS;
static EventGroupHandle_t g_mqtt_event_group;
static esp_mqtt_client_handle_t g_client = NULL;
QueueHandle_t g_mqtt_messages_received;
// QueueHandle_t g_mqtt_messages_tosend;

/**
 * @brief Handle MQTT Events inside the stack
 * @param event Event details
 */
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    static TaskHandle_t s_keepAlive = NULL;
    static TaskHandle_t s_received = NULL;
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        xEventGroupSetBits(g_mqtt_event_group, E12AIO_MQTT_CONNECTED);
        xTaskCreate(e12aio_mqtt_online_task, "mqtt_online", 2048, NULL, 5, NULL);
        xTaskCreate(e12aio_mqtt_keep_alive_task, "mqtt_keepalive", 2048, NULL, 2, &s_keepAlive);
        xTaskCreate(e12aio_mqtt_received_task, "mqtt_received", 4096, NULL, 10, &s_received);
        break;
    case MQTT_EVENT_DISCONNECTED:
        portENTER_CRITICAL();
        ESP_LOGD(TAG, "MQTT_EVENT_DISCONNECTED");
        xEventGroupClearBits(g_mqtt_event_group, E12AIO_MQTT_CONNECTED);
        if (s_keepAlive != NULL)
            vTaskDelete(s_keepAlive);
        if (s_received != NULL)
            vTaskDelete(s_received);
        portEXIT_CRITICAL();
        xTaskCreate(e12aio_mqtt_disconnect_task, "mqtt_disconnect", 2048, NULL, 5, NULL);
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
            e12aio_mqtt_received_message msg;
            memset(&msg.topic, 0, CONFIG_MQTT_TOPIC_SIZE);
            memset(&msg.payload, 0, CONFIG_MQTT_PAYLOAD_SIZE);
            memcpy(&msg.topic, event->topic, event->topic_len);
            memcpy(&msg.payload, event->data, event->data_len);
            if (xQueueSend(g_mqtt_messages_received, &msg, g_delay) != pdTRUE)
                ESP_LOGE(TAG, "Error enqueuing message: [%s] -> [%s]", msg.topic, msg.payload);
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        break;
    }
    return ESP_OK;
}

/**
 * Initialize and setup mqtt component
 */
void e12aio_mqtt_init_task(void *arg)
{
    e12aio_config_wait_load(TAG);
    ESP_LOGD(TAG, "MQTT init task called...");
    g_mqtt_messages_received = xQueueCreate(8, sizeof(e12aio_mqtt_received_message));
    // g_mqtt_messages_tosend = xQueueCreate(20, sizeof(e12aio_mqtt_received_message));
    e12aio_mqtt_connect();
    vTaskDelete(NULL);
}

/**
 * Routine called after any configuration update. Most of the time, it is a callback function.
 */
void e12aio_mqtt_restart()
{
    // Rebind all subscriptions and topics
    if (e12aio_mqtt_is_connected())
        xTaskCreate(e12aio_mqtt_disconnect_task, "mqtt_restart", 2048, NULL, 5, NULL);
}

/**
 * Connect to a MQTT Server
 */
void e12aio_mqtt_connect()
{
    ESP_LOGI(TAG, "MQTT: Connecting");
    e12aio_config_t *l_config = e12aio_config_get();
    esp_mqtt_client_config_t mqtt_cfg = {
        .event_handle = mqtt_event_handler,
        .uri = l_config->mqtt.url,
        .disable_auto_reconnect = false,
        .task_prio = 15,
        .keepalive = 60,
    };
    g_client = esp_mqtt_client_init(&mqtt_cfg);
    e12aio_wifi_sta_wait_connect(TAG);
    esp_mqtt_client_start(g_client);
}

/**
 * @brief Watchdog Disconnect Task
 * Sometimes esp_mqtt_client_stop and esp_mqtt_client_destroy freeze when wifi connection drops.
 */
void e12aio_mqtt_disconnect_watchdog(void *args)
{
    EventBits_t result = xEventGroupWaitBits(g_mqtt_event_group, E12AIO_MQTT_DISCONNECTED, pdTRUE, pdTRUE, CONFIG_MQTT_RETRY_TIMEOUT / portTICK_PERIOD_MS);
    if (!result)
        esp_restart();
    vTaskDelete(NULL);
}

/**
 * Disconnect from a MQTT Server
 */
void e12aio_mqtt_disconnect_task(void *arg)
{
    ESP_LOGI(TAG, "MQTT: Disconnecting");
    xEventGroupClearBits(g_mqtt_event_group, E12AIO_MQTT_DISCONNECTED);
    if (g_client != NULL)
    {
        xTaskCreate(e12aio_mqtt_disconnect_watchdog, "mqtt_disconnect", 2048, NULL, 3, NULL);
        esp_mqtt_client_destroy(g_client);
        g_client = NULL;
    }
    xEventGroupSetBits(g_mqtt_event_group, E12AIO_MQTT_DISCONNECTED);
    ESP_LOGD(TAG, "MQTT: Disconnected");

    // restart connecting process
    e12aio_mqtt_connect();
    vTaskDelete(NULL);
}

/**
 * FreeRTOS Task: Maintain a KeepAlive to Home Assistant
 * @param arg context FreeRTOS argument 
 */
void e12aio_mqtt_keep_alive_task(void *arg)
{
    const uint16_t l_delay = (CONFIG_MQTT_KEEPALIVE_TIMEOUT) / portTICK_PERIOD_MS;
    for (;;)
    {
        vTaskDelay(l_delay);
        ESP_LOGI(TAG, "MQTT: Sending keepAlive");
        e12aio_mqtt_keep_alive_send();
    }
    vTaskDelete(NULL);
}

/**
 * @brief Build a topic name
 * 
 * @param buffer provide a buffer to receive a topic information
 * @param sz size of buffer
 * @param type a homeassistant type, i.e. sensor, switch, action
 * @param item a item name, i.e. uptime, ipaddr, relay1
 * @param index if greater -1, append this after "item" name.
 * @param method like get, set
 */
size_t e12aio_mqtt_topic(char *buffer, size_t sz, const char *type, const char *item, int8_t index, const char *method)
{
    const char *l_config_topic = e12aio_config_get()->mqtt.topic;
    const char *l_config_name = e12aio_config_get_name();
    snprintf(buffer, sz, "%s%s/%s/%s", l_config_topic, type, l_config_name, item);
    if (index > -1)
    {
        char indexbuff[6];
        snprintf(indexbuff, 6, "%d", index);
        strncat(buffer, indexbuff, sz);
    }
    if (method != NULL)
    {
        strncat(buffer, "/", sz);
        strncat(buffer, method, sz);
    }
    return strlen(buffer);
}

size_t e12aio_mqtt_hass_sensor_config_payload(char *buffer, size_t sz, const char *name)
{
    const char *l_config_topic = e12aio_config_get()->mqtt.topic;
    const char *l_config_name = e12aio_config_get_name();
    snprintf(buffer, sz, "{ \"name\": \"%s_%s\", \"stat_t\": \"%ssensor/%s/%s\" }", l_config_name, name, l_config_topic, l_config_name, name);
    return strlen(buffer);
}

size_t e12aio_mqtt_hass_switch_config_payload(char *buffer, size_t sz, uint8_t relay)
{
    const char *l_config_topic = e12aio_config_get()->mqtt.topic;
    const char *l_config_name = e12aio_config_get_name();
    snprintf(buffer, sz,
             "{ \"~\": \"%sswitch/%s/relay%d\", \"cmd_t\": \"~/set\", \"stat_t\": \"~\", \"name\": \"%s_relay_%d\" }",
             l_config_topic, l_config_name, relay, l_config_name, relay);
    return strlen(buffer);
}

void e12aio_mqtt_hass_sensor_config(const char *name)
{
    char l_topic[CONFIG_MQTT_TOPIC_SIZE];
    char l_payload[CONFIG_MQTT_PAYLOAD_SIZE];
    e12aio_mqtt_topic(l_topic, CONFIG_MQTT_TOPIC_SIZE, "sensor", name, -1, "config");
    e12aio_mqtt_hass_sensor_config_payload(l_payload, CONFIG_MQTT_PAYLOAD_SIZE, name);
    esp_mqtt_client_publish(g_client, l_topic, l_payload, strlen(l_payload), 2, 1);
    vTaskDelay(g_delay);
}

void e12aio_mqtt_sensor_send(const char *name, const char *format, ...)
{
    char l_topic[CONFIG_MQTT_TOPIC_SIZE];
    char l_payload[CONFIG_MQTT_PAYLOAD_SIZE];
    va_list l_args;
    va_start(l_args, format);
    vsnprintf(l_payload, CONFIG_MQTT_PAYLOAD_SIZE, format, l_args);
    va_end(l_args);
    e12aio_mqtt_topic(l_topic, CONFIG_MQTT_TOPIC_SIZE, "sensor", name, -1, NULL);
    esp_mqtt_client_publish(g_client, l_topic, l_payload, strlen(l_payload), 1, 0);
    vTaskDelay(g_delay);
}

/**
 * Send all keepAlive topics to MQTT (used by Home Assistant).
 * Sample Topics and payloads:
 * home/sensor/e12aio3_CC9900/uptime, 00:01:03
 * home/sensor/e12aio3_CC9900/ipaddr, 10.10.10.10
 * home/sensor/e12aio3_CC9900/model, E12-AIO3
 * home/sensor/e12aio3_CC9900/build, v2.0.0
 */
void e12aio_mqtt_keep_alive_send()
{
    // uptime
    char l_buffer[E12AIO_UTILS_UPTIME_SIZE];
    e12aio_utils_uptime((char *)&l_buffer, E12AIO_UTILS_UPTIME_SIZE);
    e12aio_mqtt_sensor_send("uptime", l_buffer);
    e12aio_mqtt_sensor_send("ipaddr", "%s", e12aio_wifi_sta_get_ip());
    e12aio_mqtt_sensor_send("model", "E12-AIO3");
    e12aio_mqtt_sensor_send("build", E12AIO_VERSION);
    e12aio_mqtt_sensor_send("freemem", "%d", esp_get_free_heap_size());
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
void e12aio_mqtt_hass_relay(uint8_t relay)
{
    char l_topic[CONFIG_MQTT_TOPIC_SIZE];
    char l_payload[CONFIG_MQTT_PAYLOAD_SIZE];
    e12aio_mqtt_topic(l_topic, CONFIG_MQTT_TOPIC_SIZE, "switch", "relay", relay, "config");
    e12aio_mqtt_hass_switch_config_payload(l_payload, CONFIG_MQTT_PAYLOAD_SIZE, relay);
    esp_mqtt_client_publish(g_client, l_topic, l_payload, strlen(l_payload), 2, 1);
    vTaskDelay(g_delay);
}

/**
 * Send relay status to mqtt
 * Sample topic: home/switch/e12aio3_CC9900/relay1, payload: ON
 * @param relay relay number from 1 to 3
 */
void e12aio_mqtt_relay_send_status(uint8_t relay)
{
    char l_topic[CONFIG_MQTT_TOPIC_SIZE];
    char l_payload[CONFIG_MQTT_PAYLOAD_SIZE];
    strncpy(l_payload, e12aio_relay_get(relay) ? "ON" : "OFF", CONFIG_MQTT_PAYLOAD_SIZE);
    e12aio_mqtt_topic(l_topic, CONFIG_MQTT_TOPIC_SIZE, "switch", "relay", relay, NULL);
    esp_mqtt_client_publish(g_client, l_topic, l_payload, strlen(l_payload), 1, 0);
    vTaskDelay(g_delay);
}

/**
 * Subscribe in each relay topics
 * Sample topic: home/switch/e12aio3_CC9900/relay1/set
 * 
 * @param relay relay number from 1 to 3
 */
void e12aio_mqtt_relay_subscribe_status(uint8_t relay)
{
    char l_topic[CONFIG_MQTT_TOPIC_SIZE];
    e12aio_mqtt_topic(l_topic, CONFIG_MQTT_TOPIC_SIZE, "switch", "relay", relay, "set");
    esp_mqtt_client_subscribe(g_client, l_topic, 2);
}

/**
 * Method for subscribe to all relay channels
 */
void e12aio_mqtt_relay_subscribe_all()
{
    char l_topic[CONFIG_MQTT_TOPIC_SIZE];
    e12aio_mqtt_topic(l_topic, CONFIG_MQTT_TOPIC_SIZE, "switch", "+", -1, "set");
    esp_mqtt_client_subscribe(g_client, l_topic, 2);
}

/**
 * Send a configuration for sensors to home assistant.
 */
void e12aio_mqtt_hass_sensors()
{
    e12aio_mqtt_hass_sensor_config("uptime");
    e12aio_mqtt_hass_sensor_config("ipaddr");
    e12aio_mqtt_hass_sensor_config("model");
    e12aio_mqtt_hass_sensor_config("build");
    e12aio_mqtt_hass_sensor_config("freemem");
}

/**
 * Function called when connect to a MQTT Server, for send many initial informations:
 * 1. Configure Sensors
 * 2. Subscribe in each relay topic
 * 3. Configure in home assistant each relay topic
 * 4. Send to mqtt (home assistant) each relay status
 * 5. Send keepAlive: uptime, ipaddr, build, model
 */
void e12aio_mqtt_online_task(void *arg)
{
    e12aio_mqtt_hass_sensors();
    e12aio_mqtt_relay_subscribe_all();
    for (uint8_t l_x = 1; l_x < 4; l_x++)
    {
        // e12aio_mqtt_relay_subscribe_status(l_x);
        e12aio_mqtt_hass_relay(l_x);
        e12aio_mqtt_relay_send_status(l_x);
        vTaskDelay(g_delay);
    }
    e12aio_mqtt_subscribe_actions();
    e12aio_mqtt_keep_alive_send();
    vTaskDelete(NULL);
}

/**
 * Check if it is connected to a MQTT Server
 * @return connected?
 */
bool e12aio_mqtt_is_connected()
{
    return xEventGroupGetBits(g_mqtt_event_group) & E12AIO_MQTT_CONNECTED;
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
 * @param arg e12aio_mqtt_received_message contain a topic and payload
 */
void e12aio_mqtt_received_task(void *arg)
{
    e12aio_mqtt_received_message msg;
    for (;;)
    {
        if (xQueueReceive(g_mqtt_messages_received, &msg, portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGD(TAG, "Topic: %s - Payload: %s", msg.topic, msg.payload);

            // unique buffer to test any kind of topic
            size_t l_len = 0;
            char l_topic[CONFIG_MQTT_TOPIC_SIZE];

            // Discovery what kind of topic is
            if (strstr(msg.topic, "/switch/") != NULL)
            {
#ifdef CONFIG_COMPONENT_RELAY
                for (uint8_t l_x = 1; l_x < 4; l_x++)
                {
                    l_len = e12aio_mqtt_topic(l_topic, CONFIG_MQTT_TOPIC_SIZE, "switch", "relay", l_x, "set");
                    if (strncmp(msg.topic, l_topic, l_len) == 0)
                    {
                        bool l_status = strcmp(msg.payload, "ON") == 0;
                        e12aio_relay_set(l_x, l_status);
                        e12aio_mqtt_relay_send_status(l_x);
                    }
                }
#endif
            }
            else if (strstr(msg.topic, "/action/") != NULL)
            {
                // // test config/set
                // l_len = e12aio_mqtt_topic(l_topic, CONFIG_MQTT_TOPIC_SIZE, "action", "config", -1, "set");
                // if (strncmp(msg.topic, l_topic, l_len) == 0)
                // {
                //     e12aio_config_load_from_buffer(msg.payload);
                //     e12aio_config_lazy_save();
                // }
                // // test config/get
                // l_len = e12aio_mqtt_topic(l_topic, CONFIG_MQTT_TOPIC_SIZE, "action", "config", -1, "get");
                // if (strncmp(msg.topic, l_topic, l_len) == 0)
                // {
                //     char l_buffer[CONFIG_JSON_BUFFER_SIZE];
                //     e12aio_config_save_buffer(l_buffer, CONFIG_JSON_BUFFER_SIZE);
                //     e12aio_mqtt_topic(l_topic, CONFIG_MQTT_TOPIC_SIZE, "action", "config", -1, NULL);
                //     esp_mqtt_client_publish(g_client, l_topic, l_buffer, strlen(l_buffer), 1, 0);
                // }
                // test restart/set
                l_len = e12aio_mqtt_topic(l_topic, CONFIG_MQTT_TOPIC_SIZE, "action", "restart", -1, "set");
                if (strncmp(msg.topic, l_topic, l_len) == 0)
                {
                    esp_restart();
                }
            }
        }
    }
}

void e12aio_mqtt_subscribe_actions()
{
    char l_topic[CONFIG_MQTT_TOPIC_SIZE];
    e12aio_mqtt_topic(l_topic, CONFIG_MQTT_TOPIC_SIZE, "action", "#", -1, NULL);
    esp_mqtt_client_subscribe(g_client, l_topic, 2);
    vTaskDelay(g_delay);

    // e12aio_mqtt_topic(l_topic, CONFIG_MQTT_TOPIC_SIZE, "action", "config", -1, "get");
    // esp_mqtt_client_subscribe(g_client, l_topic, 2);
    // vTaskDelay(g_delay);
    // e12aio_mqtt_topic(l_topic, CONFIG_MQTT_TOPIC_SIZE, "action", "config", -1, "set");
    // esp_mqtt_client_subscribe(g_client, l_topic, 2);
    // vTaskDelay(g_delay);
    // e12aio_mqtt_topic(l_topic, CONFIG_MQTT_TOPIC_SIZE, "action", "scan", -1, "get");
    // esp_mqtt_client_subscribe(g_client, l_topic, 2);
    // vTaskDelay(g_delay);
    // e12aio_mqtt_topic(l_topic, CONFIG_MQTT_TOPIC_SIZE, "action", "restart", -1, "set");
    // esp_mqtt_client_subscribe(g_client, l_topic, 2);
    // vTaskDelay(g_delay);
}

/**
 * Main initialization, generally called in app_main after priority tasks.
 */
void e12aio_mqtt_init()
{
    g_mqtt_event_group = xEventGroupCreate();
    xTaskCreate(e12aio_mqtt_init_task, "mqtt_init", 2048, NULL, 5, NULL);
}

#else
void e12aio_mqtt_init()
{
    ESP_LOGD(TAG, "MQTT was disable in compiler configuration flags");
}
#endif
