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
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <esp_event.h>
#include <esp_event_loop.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>
#include <lwip/sys.h>
#include <lwip/sockets.h>
#include <lwip/dns.h>
#include <lwip/netdb.h>
#include <esp_log.h>
#include "wifi.h"
#include "config.h"

static const char *TAG = "wifi.c";
static EventGroupHandle_t g_wifi_event_group;

/**
 * @brief Internal function to handle wifi stages
 */
static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    /* For accessing reason codes in case of disconnection */
    system_event_info_t *l_info = &event->event_info;

    switch (event->event_id)
    {
    case SYSTEM_EVENT_STA_START:
        ESP_LOGI(TAG, "STA: trying connect");
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_SCAN_DONE:
        ESP_LOGI(TAG, "SCAN: finished!");
        xEventGroupClearBits(g_wifi_event_group, E12AIO_WIFI_SCANNING);
        xEventGroupSetBits(g_wifi_event_group, E12AIO_WIFI_SCANNING_DONE);
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "STA: got ip %s", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        xEventGroupSetBits(g_wifi_event_group, E12AIO_WIFI_CONNECTED);
        break;
    case SYSTEM_EVENT_AP_STAIPASSIGNED:
        ESP_LOGI(TAG, "AP: assigned ip %s",
                 ip4addr_ntoa(&event->event_info.ap_staipassigned.ip));
        break;
    case SYSTEM_EVENT_AP_STACONNECTED:
        ESP_LOGI(TAG, "AP: station " MACSTR " joined, AID=%d",
                 MAC2STR(event->event_info.sta_connected.mac),
                 event->event_info.sta_connected.aid);
        break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(TAG, "AP: station " MACSTR " leave, AID=%d",
                 MAC2STR(event->event_info.sta_disconnected.mac),
                 event->event_info.sta_disconnected.aid);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGE(TAG, "STA: Disconnect reason 0x%04x", l_info->disconnected.reason);
        if (l_info->disconnected.reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT)
            ESP_ERROR_CHECK(esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCAL_11B | WIFI_PROTOCAL_11G | WIFI_PROTOCAL_11N));
        xEventGroupClearBits(g_wifi_event_group, E12AIO_WIFI_CONNECTED);
        xTaskCreate(e12aio_wifi_check, "wifi_check", 4096, NULL, 5, NULL);
        break;
    default:
        break;
    }
    return ESP_OK;
}

void e12aio_wifi_init_task(void *arg)
{
    tcpip_adapter_init();
    tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, e12aio_config_get_name());
    tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_AP, e12aio_config_get_name());

    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));

    e12aio_config_wait_load(TAG);
    xTaskCreate(e12aio_wifi_check, "wifi_check", 4096, NULL, 5, NULL);
    vTaskDelete(NULL);
}

void e12aio_wifi_init()
{
    g_wifi_event_group = xEventGroupCreate();
    xTaskCreate(e12aio_wifi_init_task, "wifi_init", 2048, NULL, 5, NULL);
}

void e12aio_wifi_sta_connect()
{
    ESP_ERROR_CHECK(esp_wifi_connect());
}

bool e12aio_wifi_sta_is_available()
{
    // Async Process: start scanning
    ESP_LOGI(TAG, "SCAN: Starting");
    xEventGroupSetBits(g_wifi_event_group, E12AIO_WIFI_SCANNING);
    wifi_scan_config_t l_scan_config = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 500,
        .scan_time.active.max = 1000,
    };
    ESP_ERROR_CHECK(esp_wifi_scan_start(&l_scan_config, false));
    xEventGroupWaitBits(g_wifi_event_group, E12AIO_WIFI_SCANNING_DONE, pdTRUE, pdTRUE, portMAX_DELAY);

    // Sync Process: check for network names available
    bool l_found = false;
    uint16_t l_numAvailbleNetworks = 0, l_numRecords = CONFIG_WIFI_SCAN_MAX_NETWORKS;
    wifi_ap_record_t l_records[l_numRecords];
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&l_numRecords, l_records));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&l_numAvailbleNetworks));
    const char *l_clientSSID = e12aio_config_get()->wifi.ssid;
    for (uint16_t x = 0; x < l_numAvailbleNetworks; x++)
    {
        const char *l_SSID = (char *)(l_records[x].ssid);
        ESP_LOGI(TAG, "SCAN: [%d] Network SSID: %s, signal: %d", x, l_SSID, l_records[x].rssi);
        if (strcmp(l_clientSSID, l_SSID) == 0)
            l_found = true;
    }
    return l_found;
}

void e12aio_wifi_check(void *args)
{
    // Start AP Mode
    e12aio_wifi_ap_start();

    // Check until network is available
    const uint16_t l_delay = CONFIG_WIFI_SCAN_RETRY / portTICK_PERIOD_MS;
    bool l_isConnected = false;
    for (; !l_isConnected;)
    {
        l_isConnected = xEventGroupGetBits(g_wifi_event_group) & E12AIO_WIFI_CONNECTED;
        if (!l_isConnected)
        {
            if (e12aio_wifi_sta_is_available())
                e12aio_wifi_sta_connect();
            vTaskDelay(l_delay);
        }
    }
    // after connected, change mode to STA only
    ESP_LOGD(TAG, "STA: Changing connection to STA only");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    xEventGroupClearBits(g_wifi_event_group, E12AIO_WIFI_AP_STARTED);
    vTaskDelete(NULL);
}

void e12aio_wifi_ap_start()
{
    static bool s_wifi_started = false;
    wifi_config_t l_conf_ap;
    if (s_wifi_started)
    {
        esp_wifi_disconnect();
        esp_wifi_stop();
    }

    xEventGroupSetBits(g_wifi_event_group, E12AIO_WIFI_AP_STARTED);

    // AP Mode
    ESP_LOGI(TAG, "AP: SSID [%s], Password: [%s]", e12aio_config_get_name(), CONFIG_WIFI_AP_PASSWORD);
    memset(&l_conf_ap, 0, sizeof(wifi_config_t));
    strcpy((char *)(l_conf_ap.ap.ssid), e12aio_config_get_name());
    strcpy((char *)(l_conf_ap.ap.password), CONFIG_WIFI_AP_PASSWORD);
    l_conf_ap.ap.max_connection = CONFIG_WIFI_AP_MAX_STA_CONN;
    l_conf_ap.ap.beacon_interval = 100;
    l_conf_ap.ap.ssid_hidden = 0;
    l_conf_ap.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    l_conf_ap.ap.channel = 0;

    // STA Mode
    const e12aio_config_t *l_config = e12aio_config_get();
    ESP_LOGI(TAG, "STA: SSID [%s], Password: [%s]", l_config->wifi.ssid, l_config->wifi.password);
    wifi_config_t l_conf_sta;
    memset(&l_conf_sta, 0, sizeof(wifi_config_t));
    strcpy((char *)(l_conf_sta.sta.ssid), l_config->wifi.ssid);
    strcpy((char *)(l_conf_sta.sta.password), l_config->wifi.password);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &l_conf_ap));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &l_conf_sta));
    ESP_ERROR_CHECK(esp_wifi_start());
    s_wifi_started = true;
}

bool e12aio_wifi_ap_is_active()
{
    return (xEventGroupGetBits(g_wifi_event_group) & E12AIO_WIFI_AP_STARTED);
}

bool e12aio_wifi_sta_is_connected()
{
    return (xEventGroupGetBits(g_wifi_event_group) & E12AIO_WIFI_CONNECTED);
}

char *e12aio_wifi_generic_get_ip(tcpip_adapter_if_t interface)
{
    tcpip_adapter_ip_info_t ip;
    tcpip_adapter_get_ip_info(interface, &ip);
    return inet_ntoa(ip.ip);
}

char *e12aio_wifi_generic_get_gateway(tcpip_adapter_if_t interface)
{
    tcpip_adapter_ip_info_t ip;
    tcpip_adapter_get_ip_info(interface, &ip);
    return inet_ntoa(ip.gw);
}

char *e12aio_wifi_generic_get_netmask(tcpip_adapter_if_t interface)
{
    tcpip_adapter_ip_info_t ip;
    tcpip_adapter_get_ip_info(interface, &ip);
    return inet_ntoa(ip.netmask);
}

char *e12aio_wifi_sta_get_ip()
{
    return e12aio_wifi_generic_get_ip(TCPIP_ADAPTER_IF_STA);
}

char *e12aio_wifi_ap_get_ip()
{
    return e12aio_wifi_generic_get_ip(TCPIP_ADAPTER_IF_AP);
}

char *e12aio_wifi_sta_get_gateway()
{
    return e12aio_wifi_generic_get_gateway(TCPIP_ADAPTER_IF_STA);
}

char *e12aio_wifi_ap_get_gateway()
{
    return e12aio_wifi_generic_get_gateway(TCPIP_ADAPTER_IF_AP);
}

char *e12aio_wifi_sta_get_netmask()
{
    return e12aio_wifi_generic_get_netmask(TCPIP_ADAPTER_IF_STA);
}

char *e12aio_wifi_ap_get_netmask()
{
    return e12aio_wifi_generic_get_netmask(TCPIP_ADAPTER_IF_AP);
}

char *e12aio_wifi_get_ip()
{
    if (e12aio_wifi_sta_is_connected())
        return e12aio_wifi_sta_get_ip();
    return e12aio_wifi_ap_get_ip();
}

char *e12aio_wifi_get_gateway()
{
    if (e12aio_wifi_sta_is_connected())
        return e12aio_wifi_sta_get_gateway();
    return e12aio_wifi_ap_get_gateway();
}

char *e12aio_wifi_get_netmask()
{
    if (e12aio_wifi_sta_is_connected())
        return e12aio_wifi_sta_get_netmask();
    return e12aio_wifi_ap_get_netmask();
}

void e12aio_wifi_sta_wait_connect(const char *TAG)
{
    ESP_LOGD(TAG, "waiting until wifi connect as STA");
    xEventGroupWaitBits(g_wifi_event_group, E12AIO_WIFI_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGD(TAG, "done, connected");
}