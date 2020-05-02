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
    void subscribeActions();
    size_t buildRelaySetTopic(uint8_t relay, char *buffer, size_t sz);
    void hassConfigSensors();

public:
    MQTTClass();
    void init();
    void updateConfig();
    static void loop(void *arg);
    static void keepAlive(void *arg);
    void sendKeepAlive();
    void connect();
    void disconnect();
    bool isConnected();
    void sendOnline();
    void received(const char *topic, const char *payload);
};

extern MQTTClass MQTT;