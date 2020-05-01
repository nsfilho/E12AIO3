#pragma once
#include "mqtt_client.h"

#define D_MQTT_CONNECTED BIT0
#define D_TOPIC_SIZE 100
#define D_PAYLOAD_SIZE 5

class MQTTClass
{
private:
    esp_mqtt_client_handle_t m_client;
    void hassConfigRelay(uint8_t relay);
    void sendRelayStatus(uint8_t relay);
    bool getRelayStatus(uint8_t relay);
    void subscribeRelay(uint8_t relay);
    size_t buildRelaySetTopic(uint8_t relay, char *buffer, size_t sz);

public:
    MQTTClass();
    void init();
    static void loop(void *arg);
    static void keepAlive(void *arg);
    void connect();
    void disconnect();
    bool isConnected();
    void sendOnline();
    void sendAlive();
    void received(const char *topic, const char *payload);
};

extern MQTTClass MQTT;