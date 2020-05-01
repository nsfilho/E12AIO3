#pragma once
#include "freertos/FreeRTOS.h"

class RelayClass
{
public:
    RelayClass();
    void init();
    void setStatus(uint8_t relay, bool status);
    bool getStatus(uint8_t relay);
};

extern RelayClass Relay;