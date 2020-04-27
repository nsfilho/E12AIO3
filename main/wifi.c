#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "esp_log.h"
#include "wifi.h"
#include "config.h"

static const char *TAG = "wifi.c";
static EventGroupHandle_t g_wifi_event_group;

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    /* For accessing reason codes in case of disconnection */
    system_event_info_t *info = &event->event_info;

    switch (event->event_id)
    {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_SCAN_DONE:
        ESP_LOGI(TAG, "Scan finished");
        xEventGroupClearBits(g_wifi_event_group, D_WIFI_SCANNING);
        wifi_check_available_networks();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "got ip:%s", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        xEventGroupSetBits(g_wifi_event_group, D_WIFI_CONNECTED);
        break;
    case SYSTEM_EVENT_AP_STAIPASSIGNED:
        ESP_LOGI(TAG, "assigned ip:%s",
                 ip4addr_ntoa(&event->event_info.ap_staipassigned.ip));
        break;
    case SYSTEM_EVENT_AP_STACONNECTED:
        ESP_LOGI(TAG, "station:" MACSTR " join, AID=%d",
                 MAC2STR(event->event_info.sta_connected.mac),
                 event->event_info.sta_connected.aid);
        break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(TAG, "station:" MACSTR "leave, AID=%d",
                 MAC2STR(event->event_info.sta_disconnected.mac),
                 event->event_info.sta_disconnected.aid);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGE(TAG, "Disconnect reason : %d", info->disconnected.reason);
        if (info->disconnected.reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT)
        {
            /*Switch to 802.11 bgn mode */
            esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCAL_11B | WIFI_PROTOCAL_11G | WIFI_PROTOCAL_11N);
        }
        esp_wifi_connect();
        xEventGroupClearBits(g_wifi_event_group, D_WIFI_CONNECTED);
        break;
    default:
        break;
    }
    return ESP_OK;
}

void wifi_scan_network()
{
    // wifi_scan_config_t l_config = {
    //     .ssid = 0,
    //     .bssid = 0,

    // };
    // xEventGroupSetBits(wifi_event_group, WIFI_SCANNING);
    // ESP_ERROR_CHECK(esp_wifi_scan_start(&l_config, false));
}

void wifi_check_available_networks()
{
    // bool l_found = false;
    // const char *l_findNetwork = (const char *)&(config_wifi->sta.ssid);
    // uint16_t l_numAvailbleNetworks;
    // ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&l_numAvailbleNetworks));
    // for (uint16_t x = 0; x < l_numAvailbleNetworks; x++)
    // {
    //     wifi_ap_record_t net;
    //     ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&x, &net));
    //     ESP_LOGI(TAG, "[%d] Network SSID: %s, signal: %d", x, net.ssid, net.rssi);
    //     if (strcmp(l_findNetwork, (const char *)&net.ssid) == 0)
    //         l_found = true;
    // }
    // if (l_found)
    // {
    //     ESP_LOGI(TAG, "Configured WiFi is available, connecting...");
    //     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    //     ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, config_wifi));
    //     ESP_ERROR_CHECK(esp_wifi_start());
    // }
}

void wifi_init()
{
    g_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    // Waiting until all configs are loaded.
    config_wait_loaded();
}