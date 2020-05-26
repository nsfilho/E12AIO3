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
#include <driver/gpio.h>
#include <esp_log.h>
#include "config.h"
#include "relay.h"

#ifdef CONFIG_COMPONENT_RELAY
static const char *TAG = "relay.cpp";

void e12aio_relay_init_task(void *arg)
{
    ESP_LOGI(TAG, "Setting relay ports");
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT(CONFIG_RELAY_PORT1) | BIT(CONFIG_RELAY_PORT2) | BIT(CONFIG_RELAY_PORT3);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    e12aio_config_wait_load(TAG);
    e12aio_config_t *l_config = e12aio_config_get();
    ESP_LOGI(TAG, "Initializing ports on previous state [%d,%d,%d]",
             l_config->relay.port1 ? 1 : 0,
             l_config->relay.port2 ? 1 : 0,
             l_config->relay.port3 ? 1 : 0);
    gpio_set_level(RELAY1, l_config->relay.port1 ? 1 : 0);
    gpio_set_level(RELAY2, l_config->relay.port2 ? 1 : 0);
    gpio_set_level(RELAY3, l_config->relay.port3 ? 1 : 0);
    vTaskDelete(NULL);
}
#endif

void e12aio_relay_init()
{
#ifdef CONFIG_COMPONENT_RELAY
    xTaskCreate(e12aio_relay_init_task, "relay_init", 2048, NULL, 5, NULL);
#endif
}

#ifdef CONFIG_COMPONENT_RELAY
void e12aio_relay_set(uint8_t relay, bool status)
{
    if (e12aio_relay_get(relay) != status)
    {
        ESP_LOGD(TAG, "Changing relay [%d] to [%d]", relay, status ? 1 : 0);
        switch (relay)
        {
        case 1:
            gpio_set_level(RELAY1, status ? 1 : 0);
            break;
        case 2:
            gpio_set_level(RELAY2, status ? 1 : 0);
            break;
        case 3:
            gpio_set_level(RELAY3, status ? 1 : 0);
            break;
        }
        e12aio_config_relay_set(relay, status);
    }
}

bool e12aio_relay_get(uint8_t relay)
{
    return *e12aio_config_relay_pointer(relay);
}
#endif