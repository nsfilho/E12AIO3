#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"

static EventGroupHandle_t config_event_group;

#define CONFIG_LOADED BIT0
#define CONFIG_SAVED BIT1

void config_init();
void config_name();
void config_load();