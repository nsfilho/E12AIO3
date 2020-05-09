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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <fcntl.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "wifi.hpp"
#include "config.hpp"
#include "httpd.hpp"
#include <vector>

static const char *TAG = "httpd.cpp";
static std::vector<httpd_uri_t> pages;

HTTPDClass::HTTPDClass()
{
}

void HTTPDClass::init()
{
    Config.waitUntilLoad(TAG);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    xTaskCreate(HTTPD.loop, "httpd_loop", 2048, NULL, 5, NULL);
}

void HTTPDClass::loop(void *arg)
{
    const uint16_t delay = CONFIG_WIFI_SCAN_RETRY / portTICK_PERIOD_MS;
    static bool lastWifi_status = false;
    HTTPD.start();
    for (;;)
    {
        if (WIFI.isConnected() != lastWifi_status)
        {
            // recria o servidor
            ESP_LOGI(TAG, "Restarting HTTPD server...");
            HTTPD.stop();
            vTaskDelay(100 / portTICK_PERIOD_MS);
            HTTPD.start();
            lastWifi_status = WIFI.isConnected();
        }
        vTaskDelay(delay);
    }
    vTaskDelete(NULL);
}

esp_err_t HTTPDClass::spiffsHandler(httpd_req_t *req)
{
    char filename[32];
    char l_buffer[CONFIG_JSON_BUFFER_SIZE];
    snprintf(filename, 32, "/spiffs/%s", (const char *)req->user_ctx);
    ESP_LOGI(TAG, "Serving page file: %s", filename);
    FILE *l_fp = fopen(filename, "r");
    if (l_fp == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file: %s for reading", filename);
        return ESP_ERR_NOT_FOUND;
    }
    size_t readBytes = 0;
    do
    {
        readBytes = fread(&l_buffer, 1, CONFIG_JSON_BUFFER_SIZE, l_fp);
        httpd_resp_send_chunk(req, l_buffer, readBytes);
    } while (readBytes == CONFIG_JSON_BUFFER_SIZE);
    fclose(l_fp);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

void HTTPDClass::start()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&this->m_server, &config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");

        struct dirent *de;
        DIR *dir = opendir("/spiffs");
        if (dir == NULL)
        {
            ESP_LOGE(TAG, "Fail to open /spiffs directory.");
        }
        else
        {
            while ((de = readdir(dir)) != NULL)
            {
                ESP_LOGD(TAG, "SPIFF File: %s", de->d_name);
                httpd_uri_t uri = {
                    .uri = (std::string("/") + std::string(de->d_name)).c_str(),
                    .method = HTTP_GET,
                    .handler = this->spiffsHandler,
                    .user_ctx = strdup(de->d_name),
                };
                pages.push_back(uri);
                httpd_register_uri_handler(this->m_server, &uri);
            }
            closedir(dir);
        }
    }
    else
    {
        ESP_LOGI(TAG, "Error starting server!");
    }
}

void HTTPDClass::stop()
{
    // Stop the httpd server
    httpd_stop(this->m_server);
    for (uint8_t x = 0; x < pages.size(); x++)
    {
        free(pages[x].user_ctx);
    }
    pages.clear();
}

HTTPDClass HTTPD;