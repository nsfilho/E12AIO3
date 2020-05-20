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
#include <esp_wifi.h>

#define E12AIO_WIFI_CONNECTED BIT0
#define E12AIO_WIFI_SCANNING BIT1
#define E12AIO_WIFI_SCANNING_DONE BIT2
#define E12AIO_WIFI_AP_STARTED BIT3

void e12aio_wifi_init();
void e12aio_wifi_check(void *args);
char *e12aio_wifi_get_ip();
char *e12aio_wifi_get_gateway();
char *e12aio_wifi_get_netmask();
char *e12aio_wifi_generic_get_ip(tcpip_adapter_if_t interface);
char *e12aio_wifi_generic_get_gateway(tcpip_adapter_if_t interface);
char *e12aio_wifi_generic_get_netmask(tcpip_adapter_if_t interface);

void e12aio_wifi_sta_connect();
bool e12aio_wifi_sta_is_available();
bool e12aio_wifi_sta_is_connected();
char *e12aio_wifi_sta_get_ip();
char *e12aio_wifi_sta_get_gateway();
char *e12aio_wifi_sta_get_netmask();
void e12aio_wifi_sta_wait_connect(const char *TAG);

void e12aio_wifi_ap_start();
bool e12aio_wifi_ap_is_active();
char *e12aio_wifi_ap_get_ip();
char *e12aio_wifi_ap_get_gateway();
char *e12aio_wifi_ap_get_netmask();
void e12aio_wifi_ap_wait_deactive(const char *TAG);
