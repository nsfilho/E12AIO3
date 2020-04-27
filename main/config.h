#pragma once
#include "freertos/FreeRTOS.h"

#define D_CONFIG_BUFFER_SIZE 512
#define D_CONFIG_LOADED BIT0
#define D_CONFIG_SAVED BIT1
#define D_CONFIG_DELAYED_SAVE BIT2

typedef struct
{
    char wifi_ssid[33];
    char wifi_password[33];
    char mqtt_url[50];
    bool relay1;
    bool relay2;
    bool relay3;
} config_t;

void config_init();
void config_esp_name();
void config_prepare_spiffs();
void config_prepare_configs();
void config_load();
void config_load_inMemory(const char *buffer);
void config_save();
void config_lazy_save();
size_t config_save_inMemory(config_t data, char *buffer, size_t sz);
void config_dump();