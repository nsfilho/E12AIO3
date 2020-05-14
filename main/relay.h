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
#include <driver/gpio.h>

// #define RELAY1 GPIO_NUM_14
// #define RELAY2 GPIO_NUM_12
// #define RELAY3 GPIO_NUM_13
#define RELAY1 CONFIG_RELAY_PORT1
#define RELAY2 CONFIG_RELAY_PORT2
#define RELAY3 CONFIG_RELAY_PORT3

void e12aio_relay_init();
void e12aio_relay_set(uint8_t relay, bool status);
bool e12aio_relay_get(uint8_t relay);