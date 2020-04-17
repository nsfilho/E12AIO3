#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"

static EventGroupHandle_t config_event_group;

#define CONFIG_LOADED BIT0
#define CONFIG_SAVED BIT1

static wifi_config_t config_wifi_ap = {
    .ap = {
        .password = CONFIG_WIFI_AP_PASSWORD,
    },
};

static wifi_config_t config_wifi_sta = {
    .sta = {
        .ssid = CONFIG_WIFI_SSID,
        .password = CONFIG_WIFI_PASSWORD,
    },
};

void config_init();
void config_name();
void config_load();