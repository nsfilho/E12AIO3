#include "relay.hpp"
#include "config.hpp"

RelayClass::RelayClass()
{
}

void RelayClass::init()
{
}

void RelayClass::setStatus(uint8_t relay, bool status)
{
    if (this->getStatus(relay) != status)
    {
        // troca o estado da porta.
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