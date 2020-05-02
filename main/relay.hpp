#pragma once
#include "freertos/FreeRTOS.h"
#include <driver/gpio.h>

#define RELAY1 GPIO_NUM_14
#define RELAY2 GPIO_NUM_12
#define RELAY3 GPIO_NUM_13

class RelayClass
{
public:
    RelayClass();
    void init();
    void setStatus(uint8_t relay, bool status);
    bool getStatus(uint8_t relay);
};

extern RelayClass Relay;