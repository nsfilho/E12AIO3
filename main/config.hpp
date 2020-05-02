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
#include "freertos/FreeRTOS.h"
#include <string>

#define D_CONFIG_LOADED BIT0
#define D_CONFIG_SAVED BIT1
#define D_CONFIG_DELAYED_SAVE BIT2

/**
 * Struct that contains all wifi information.
 */
typedef struct
{
    std::string ssid;
    std::string password;
} wifi_t;

/**
 * Struct that contains all mqtt informations
 */
typedef struct
{
    std::string url;
    std::string topic;
} mqtt_t;

/**
 * Struct that control all relays status
 */
typedef struct
{
    bool port1;
    bool port2;
    bool port3;
} relays_t;

/**
 * Union of all config information
 */
typedef struct
{
    wifi_t wifi;
    relays_t relay;
    mqtt_t mqtt;
} config_t;

class ConfigClass
{
private:
    void prepare_spiffs();
    void prepare_configs();
    void prepare_name();

public:
    ConfigClass();

    /** Initialize component */
    void init();

    /** Load all configs from spiffs */
    void load();
    /** Load configs from buffer (read json) */
    void loadInMemory(const char *buffer);
    /** Save all configs in spiffs */
    void save();
    /** Save all changes after certain amount of time */
    void lazySave();

    /** Save all configs (in use) in a buffer (generate json) */
    size_t saveInMemory(char *buffer, size_t sz);
    /** Save all configs in a buffer (generate json) */
    size_t saveInMemory(config_t data, char *buffer, size_t sz);
    /** Dumping all knowed configurations */
    void dump();

    /** Get a ESP Name */
    std::string getName();
    /** Wait until all configuration is loaded */
    void waitUntilLoad(const char *TAG);

    /** Read configuration values */
    config_t getValues();
    void setRelayStatus(uint8_t relay, bool status);
    /** Update (from mqtt or web) configuration values */
    void updateValues(const char *buffer);
};

extern ConfigClass Config;