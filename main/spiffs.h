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

void e12aio_spiffs_init();
char *e12aio_spiffs_get_basepath();
bool e12aio_spiffs_has_basepath(char *filename);
char *e12aio_spiffs_fullpath(char *filename);
size_t e12aio_spiffs_read(char *filename, char *buffer, size_t sz);
size_t e12aio_spiffs_write(char *filename, char *buffer, size_t sz);
