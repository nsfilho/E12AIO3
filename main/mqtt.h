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

#define E12AIO_MQTT_CONNECTED BIT0
#define E12AIO_MQTT_DISCONNECTED BIT1

void e12aio_mqtt_init_task();
void e12aio_mqtt_init();
void e12aio_mqtt_restart();
void e12aio_mqtt_connect();
void e12aio_mqtt_disconnect();
void e12aio_mqtt_online_send();
void e12aio_mqtt_received(const char *topic, const char *payload);
void e12aio_mqtt_subscribe_actions();
void e12aio_mqtt_keep_alive_task(void *arg);
void e12aio_mqtt_keep_alive_send();
void e12aio_mqtt_relay_send_status(uint8_t relay);
void e12aio_mqtt_relay_subscribe_status(uint8_t relay);
size_t e12aio_mqtt_relay_topic_set(uint8_t relay, char *buffer, size_t sz);
void e12aio_mqtt_hass_relay(uint8_t relay);
void e12aio_mqtt_hass_sensors();
bool e12aio_mqtt_is_connected();
