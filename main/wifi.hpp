#pragma once

#define D_WIFI_CONNECTED BIT0
#define D_WIFI_SCANNING BIT1
#define D_WIFI_SCANNING_DONE BIT2
#define D_WIFI_AP_STARTED BIT3

class WIFIClass
{
public:
    WIFIClass();
    void init();
    void startConnect();
    void startAP();
    static void checkNetwork(void *args);
    bool networkAvailable();
    bool isAPModeActive();
    bool isConnected();
};

extern WIFIClass WIFI;