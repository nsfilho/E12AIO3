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