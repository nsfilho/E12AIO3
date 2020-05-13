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
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include <esp_system.h>
#include "e12aio.h"
#include "config.h"
#include "wifi.h"
#include "relay.h"
#include "mqtt.h"

#define DEBUG_TASK

static const char *TAG = "e12aio.c";

#ifdef DEBUG_TASK
void debug_task(void *arg)
{
    for (;;)
    {
        ESP_LOGW(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}
#endif

void nvs_init()
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void app_main()
{
    ESP_LOGI(TAG, "[APP] Starting E12-AIO3 Firmware...");
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    nvs_init();
#ifdef DEBUG_TASK
    xTaskCreate(debug_task, "debug_task", 2048, NULL, 1, NULL);
#endif
    e12aio_config_init();
    e12aio_wifi_init();
    e12aio_relay_init();
    // HTTPD.init();
    e12aio_mqtt_init();
}