#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_system.h>
#include "e12aio.h"
#include "config.h"
#include "ota.h"
#include "mqtt.h"
#include "httpd.h"
#include "wifi.h"

#ifdef CONFIG_COMPONENT_OTA
static const char *TAG = "ota.c";
#ifdef CONFIG_OTA_LOCAL_CERTIFICATE
extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");
#endif
extern const uint8_t server_github_start[] asm("_binary_github_pem_start");
extern const uint8_t server_github_end[] asm("_binary_github_pem_end");

esp_err_t e12aio_ota_http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    }
    return ESP_OK;
}

void e12aio_ota_watchdog(void *args)
{
    vTaskDelay(CONFIG_OTA_WATCHDOG_TIMEOUT / portTICK_PERIOD_MS);
    e12aio_config_get()->ota.state = E12AIO_OTA_FAILED_WATCHDOG;
    e12aio_config_save();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();
}

void e12aio_ota_task(void *args)
{
    // #ifdef CONFIG_COMPONENT_HTTPD
    //     ESP_LOGI(TAG, "Stoping HTTPD Component");
    //     e12aio_httpd_stop();
    // #endif
    // #ifdef CONFIG_COMPONENT_MQTT
    //     ESP_LOGI(TAG, "Stopping MQTT Component");
    //     e12aio_mqtt_disconnect();
    // #endif
    ESP_LOGI(TAG, "Starting OTA Update: %s", e12aio_config_get()->ota.url);
    xTaskCreate(e12aio_ota_watchdog, "ota_watchdog", 2048, NULL, 5, NULL);
    e12aio_wifi_sta_wait_connect(TAG);
    e12aio_wifi_ap_wait_deactive(TAG);
#ifdef CONFIG_OTA_LOCAL_CERTIFICATE
    char *cert_pem = strstr(e12aio_config_get()->ota.url, "github.com/") == NULL ? (char *)server_cert_pem_start : NULL;
#else
    char *cert_pem = NULL;
#endif
    esp_http_client_config_t config = {
        .url = e12aio_config_get()->ota.url,
        .cert_pem = cert_pem,
        .timeout_ms = 180000,
        .event_handler = e12aio_ota_http_event_handler,
    };
    esp_err_t ret = esp_https_ota(&config);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Firmware Updated!");
        e12aio_config_get()->ota.state = E12AIO_OTA_OK;
    }
    else
    {
        ESP_LOGE(TAG, "Firmware Upgrades Failed");
        e12aio_config_get()->ota.state = E12AIO_OTA_FAILED_DOWNLOAD;
    }
    e12aio_config_save();
    vTaskDelay(100 / portTICK_PERIOD_MS);
    esp_restart();
}

bool e12aio_ota_init()
{
    e12aio_config_wait_load(TAG);
    switch (e12aio_config_get()->ota.state)
    {
    case E12AIO_OTA_SCHEDULED:
        e12aio_config_get()->ota.state = E12AIO_OTA_STARTED;
        e12aio_config_save();
        xTaskCreate(e12aio_ota_task, "ota_task", 8192, NULL, 20, NULL);
        return true;
        break;
    case E12AIO_OTA_STARTED:
        e12aio_config_get()->ota.state = E12AIO_OTA_FAILED_OTHER;
        e12aio_config_lazy_save();
        break;
    default:
        break;
    }
    return false;
}

void e12aio_ota_start(const char *url)
{
    strncpy(e12aio_config_get()->ota.version, E12AIO_VERSION, E12AIO_OTA_VERSION_SIZE);
    strncpy(e12aio_config_get()->ota.url, url, E12AIO_OTA_URL_SIZE);
    e12aio_config_get()->ota.state = E12AIO_OTA_SCHEDULED;
    e12aio_config_save();
    vTaskDelay(100 / portTICK_PERIOD_MS);
    esp_restart();
}

#endif