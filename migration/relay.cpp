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
#include "esp_log.h"
#include "driver/gpio.h"
#include "relay.hpp"
#include "config.hpp"

static const char *TAG = "relay.cpp";

RelayClass::RelayClass()
{
}

void RelayClass::init()
{
    ESP_LOGD(TAG, "Setup relay ports...");
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_Pin_14 | GPIO_Pin_12 | GPIO_Pin_13;
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    Config.waitUntilLoad(TAG);
    ESP_LOGI(TAG, "Initializing ports on previous state [%d,%d,%d]",
             Config.getValues().relay.port1 ? 1 : 0,
             Config.getValues().relay.port2 ? 1 : 0,
             Config.getValues().relay.port3 ? 1 : 0);
    gpio_set_level(RELAY1, Config.getValues().relay.port1 ? 1 : 0);
    gpio_set_level(RELAY2, Config.getValues().relay.port2 ? 1 : 0);
    gpio_set_level(RELAY3, Config.getValues().relay.port3 ? 1 : 0);
}

void RelayClass::setStatus(uint8_t relay, bool status)
{
    if (this->getStatus(relay) != status)
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
        Config.setRelayStatus(relay, status);
        Config.lazySave();
    }
}

bool RelayClass::getStatus(uint8_t relay)
{
    bool l_status = false;
    switch (relay)
    {
    case 1:
        l_status = Config.getValues().relay.port1;
        break;
    case 2:
        l_status = Config.getValues().relay.port2;
        break;
    case 3:
        l_status = Config.getValues().relay.port3;
        break;
    }
    return l_status;
}

RelayClass Relay;