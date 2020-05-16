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
#include <esp_http_server.h>

#define E12AIO_MAX_FILENAME 32

void e12aio_httpd_init();
bool e12aio_httpd_auth_valid(httpd_req_t *req);
bool e12aio_httpd_auth_ok(httpd_req_t *req);
esp_err_t e12aio_httpd_spiffs_handler(httpd_req_t *req);
esp_err_t e12aio_httpd_handler_save(httpd_req_t *req);
esp_err_t e12aio_httpd_handler_status(httpd_req_t *req);
esp_err_t e12aio_httpd_handler_action(httpd_req_t *req);
esp_err_t e12aio_httpd_handler_api(httpd_req_t *req);
void e12aio_httpd_register_index();
void e12aio_httpd_register_save();
void e12aio_httpd_register_status();
void e12aio_httpd_register_action();
void e12aio_httpd_register_api();
void e12aio_httpd_start();
void e12aio_httpd_stop();
