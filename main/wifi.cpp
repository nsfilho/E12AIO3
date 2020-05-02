#include <iostream>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "esp_log.h"
#include "wifi.hpp"
#include "config.hpp"

static const char *TAG = "wifi.c";
static EventGroupHandle_t g_wifi_event_group;
static bool g_wifi_started = false;

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    /* For accessing reason codes in case of disconnection */
    system_event_info_t *l_info = &event->event_info;

    switch (event->event_id)
    {
    case SYSTEM_EVENT_STA_START:
        ESP_LOGD(TAG, "STA: trying connect to AP...");
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_SCAN_DONE:
        ESP_LOGI(TAG, "SCAN: finished!");
        xEventGroupClearBits(g_wifi_event_group, D_WIFI_SCANNING);
        xEventGroupSetBits(g_wifi_event_group, D_WIFI_SCANNING_DONE);
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "STA: got ip: %s", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        xEventGroupSetBits(g_wifi_event_group, D_WIFI_CONNECTED);
        break;
    case SYSTEM_EVENT_AP_STAIPASSIGNED:
        ESP_LOGI(TAG, "AP: assigned ip: %s",
                 ip4addr_ntoa(&event->event_info.ap_staipassigned.ip));
        break;
    case SYSTEM_EVENT_AP_STACONNECTED:
        ESP_LOGI(TAG, "AP: station " MACSTR " join, AID=%d",
                 MAC2STR(event->event_info.sta_connected.mac),
                 event->event_info.sta_connected.aid);
        break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(TAG, "AP: station " MACSTR "leave, AID=%d",
                 MAC2STR(event->event_info.sta_disconnected.mac),
                 event->event_info.sta_disconnected.aid);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGE(TAG, "STA: Disconnect reason : 0x%04x", l_info->disconnected.reason);
        if (l_info->disconnected.reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT)
            ESP_ERROR_CHECK(esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCAL_11B | WIFI_PROTOCAL_11G | WIFI_PROTOCAL_11N));
        xEventGroupClearBits(g_wifi_event_group, D_WIFI_CONNECTED);

        // Create a SoftAP if is need until connection is ready again.
        if (!WIFI.isAPModeActive())
            xTaskCreate(WIFI.checkNetwork, "checkNetwork", 4096, NULL, 5, NULL);
        break;
    default:
        break;
    }
    return ESP_OK;
}

WIFIClass::WIFIClass()
{
}

void WIFIClass::init()
{
    g_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, Config.getName().c_str());
    tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_AP, Config.getName().c_str());
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    Config.waitUntilLoad(TAG);
    xTaskCreate(WIFI.checkNetwork, "checkNetwork", 4096, NULL, 5, NULL);
}

void WIFIClass::startConnect()
{
    // Check if the network is available and connect
    ESP_ERROR_CHECK(esp_wifi_connect());
    // ESP_ERROR_CHECK(tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_STA));
}

bool WIFIClass::networkAvailable()
{
    // Async Process: start scanning
    ESP_LOGI(TAG, "SCAN: Starting...");
    xEventGroupSetBits(g_wifi_event_group, D_WIFI_SCANNING);
    ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, false));
    xEventGroupWaitBits(g_wifi_event_group, D_WIFI_SCANNING_DONE, pdTRUE, pdTRUE, portMAX_DELAY);

    // Sync Process: check for network names available
    bool l_found = false;
    uint16_t l_numAvailbleNetworks = 0, l_numRecords = CONFIG_WIFI_SCAN_MAX_NETWORKS;
    wifi_ap_record_t l_records[l_numRecords];
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&l_numRecords, l_records));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&l_numAvailbleNetworks));
    for (uint16_t x = 0; x < l_numAvailbleNetworks; x++)
    {
        const char *l_clientSSID = Config.getValues().wifi.ssid.c_str();
        const char *l_SSID = reinterpret_cast<char *>(l_records[x].ssid);
        ESP_LOGI(TAG, "SCAN: [%d] Network SSID: %s, signal: %d", x, l_SSID, l_records[x].rssi);
        if (strcmp(l_clientSSID, l_SSID) == 0)
            l_found = true;
    }
    return l_found;
}

void WIFIClass::checkNetwork(void *args)
{
    // Start AP Mode
    WIFI.startAP();

    // Check until network is available
    const uint16_t l_delay = CONFIG_WIFI_SCAN_RETRY / portTICK_PERIOD_MS;
    bool l_isConnected = false;
    for (; !l_isConnected;)
    {
        l_isConnected = xEventGroupGetBits(g_wifi_event_group) & D_WIFI_CONNECTED;
        if (!l_isConnected)
        {
            if (WIFI.networkAvailable())
            {
                ESP_LOGD(TAG, "STA: Trying to connect to %s", Config.getValues().wifi.ssid.c_str());
                WIFI.startConnect();
            }
            vTaskDelay(l_delay);
        }
        else
        {
            // after connected, change mode to STA only
            ESP_LOGD(TAG, "STA: Changing connection to mode STA only");
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            xEventGroupClearBits(g_wifi_event_group, D_WIFI_AP_STARTED);
        }
    }
    vTaskDelete(NULL);
}

void WIFIClass::startAP()
{
    if (g_wifi_started)
    {
        esp_wifi_disconnect();
        esp_wifi_stop();
    }

    xEventGroupSetBits(g_wifi_event_group, D_WIFI_AP_STARTED);
    wifi_config_t l_conf_ap;
    wifi_config_t l_conf_sta;
    ESP_LOGI(TAG, "AP: Starting mode, network [%s]", Config.getName().c_str());
    memset(&l_conf_ap, 0, sizeof(wifi_config_t));
    memset(&l_conf_sta, 0, sizeof(wifi_config_t));
    ESP_LOGD(TAG, "AP: SSID [%s], Password: [%s]", Config.getName().c_str(), CONFIG_WIFI_AP_PASSWORD);
    strcpy(reinterpret_cast<char *>(l_conf_ap.ap.ssid), Config.getName().c_str());
    strcpy(reinterpret_cast<char *>(l_conf_ap.ap.password), CONFIG_WIFI_AP_PASSWORD);
    ESP_LOGD(TAG, "STA: SSID [%s], Password: [%s]", Config.getValues().wifi.ssid.c_str(), Config.getValues().wifi.password.c_str());
    strcpy(reinterpret_cast<char *>(l_conf_sta.sta.ssid), Config.getValues().wifi.ssid.c_str());
    strcpy(reinterpret_cast<char *>(l_conf_sta.sta.password), Config.getValues().wifi.password.c_str());
    l_conf_ap.ap.max_connection = CONFIG_WIFI_AP_MAX_STA_CONN;
    l_conf_ap.ap.beacon_interval = 100;
    l_conf_ap.ap.ssid_hidden = 0;
    l_conf_ap.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    l_conf_ap.ap.channel = 0;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &l_conf_ap));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &l_conf_sta));
    ESP_ERROR_CHECK(esp_wifi_start());
    g_wifi_started = true;
}

bool WIFIClass::isAPModeActive()
{
    return (xEventGroupGetBits(g_wifi_event_group) & D_WIFI_AP_STARTED);
}

bool WIFIClass::isConnected()
{
    return (xEventGroupGetBits(g_wifi_event_group) & D_WIFI_CONNECTED);
}

char *WIFIClass::getStationIP()
{
    tcpip_adapter_ip_info_t ip;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip);
    return inet_ntoa(ip.ip);
}

WIFIClass WIFI;
