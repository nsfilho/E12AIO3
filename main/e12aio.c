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
#include <nvs_flash.h>
#include <esp_log.h>
#include "e12aio.h"
#include "debug.h"
#include "config.h"
#include "wifi.h"
#include "relay.h"
#include "mqtt.h"
#include "httpd.h"
#include "ota.h"

static const char *TAG = "e12aio.c";

void nvs_init()
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NOT_INITIALIZED)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void app_main()
{
    ESP_LOGI(TAG, "[APP] Starting E12-AIO3 Firmware...");
    nvs_init();

    // start block
    e12aio_config_init(); // async
    e12aio_wifi_init();   // async
    if (!e12aio_ota_init())
    {
        // rest of all
        e12aio_debug_init(); // async
        e12aio_relay_init(); // async
        e12aio_mqtt_init();  // async
        e12aio_httpd_init(); // async
    }
    // end
    vTaskDelete(NULL);
}