#pragma once

class MQTTClass
{
public:
    void init();
    static void loop(void *arg);
    static void keepAlive(void *arg);
    void connect();
    void disconnect();
};

extern MQTTClass MQTT;