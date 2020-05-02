/**
 * E12AIO3 Board (c) 2020-04
 * Nelio Santos <nsfilho@icloud.com>
 * 
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "rom/ets_sys.h"
#include "config.hpp"
#include "wifi.hpp"
#include "mqtt.hpp"
#include "relay.hpp"

static const char *TAG = "e12aio.c";

extern "C"
{
    void app_main()
    {
        ESP_LOGI(TAG, "[APP] Starting E12-AIO3 Firmware...");
        ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
        ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

        //Initialize NVS
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES)
        {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);
        Config.init();
        WIFI.init();
        MQTT.init();
        Relay.init();
    }
}