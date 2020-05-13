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
#include <freertos/FreeRTOS.h>

#define E12AIO_CONFIG_LOADED BIT0
#define E12AIO_CONFIG_SAVED BIT1
#define E12AIO_CONFIG_DELAYED_SAVE BIT2

/**
 * Struct that contains all wifi information.
 */
typedef struct
{
    char ssid[32];
    char password[64];
} e12aio_config_wifi_t;

/**
 * Struct that contains all mqtt informations
 */
typedef struct
{
    char url[100];
    char topic[CONFIG_MQTT_TOPIC_SIZE];
} e12aio_config_mqtt_t;

/**
 * Struct that control all relays status
 */
typedef struct
{
    bool port1;
    bool port2;
    bool port3;
} e12aio_config_relays_t;

/**
 * Union of all config information
 */
typedef struct
{
    e12aio_config_wifi_t wifi;
    e12aio_config_relays_t relay;
    e12aio_config_mqtt_t mqtt;
} e12aio_config_t;

void e12aio_config_init();
char *e12aio_config_get_name();
void e12aio_config_prepare_spiffs();
void e12aio_config_prepare_configs();
void e12aio_config_lazy_save();
void e12aio_config_load();
void e12aio_config_load_from_buffer(const char *buffer);
void e12aio_config_save();
size_t e12aio_config_save_buffer(char *buffer, size_t sz);
size_t e12aio_config_save_buffer_adv(e12aio_config_t data, char *buffer, size_t sz);
void e12aio_config_dump();
void e12aio_config_wait_load(const char *TAG);
e12aio_config_t *e12aio_config_get();
void e12aio_config_update_from_buffer(const char *buffer);
bool *e12aio_config_relay_pointer(uint8_t relay);
void e12aio_config_relay_set(uint8_t relay, bool status);
void e12aio_config_relay_set_adv(uint8_t relay, bool status, bool save);