#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_system.h>
#include <esp_tls.h>
#include "e12aio.h"
#include "config.h"
#include "ota.h"
#include "httpd.h"
#include "wifi.h"
#include "spiffs.h"

#ifdef CONFIG_COMPONENT_OTA
static const char *TAG = "ota.c";
char *ota_file_url_firmware = "/v/firmware.txt";
char *ota_file_url_spiffs = "/v/spiffs.txt";

extern const uint8_t server_certs_start[] asm("_binary_allcerts_pem_start");
extern const uint8_t server_certs_end[] asm("_binary_allcerts_pem_end");

/**
 * @brief HTTP Client Event Handler
 * 
 * In this OTA process, it is only a sugar for debug (not used in real).
 */
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

/**
 * @brief Watchdog Task for control stalled
 * 
 * If in any part of the process, ESP8266 comes stalled, this function will
 * reset then, after save some status details about that.
 * 
 */
void e12aio_ota_watchdog(void *args)
{
    vTaskDelay(CONFIG_OTA_WATCHDOG_TIMEOUT / portTICK_PERIOD_MS);
    e12aio_config_get()->ota.state = E12AIO_OTA_FAILED_WATCHDOG;
    e12aio_config_save();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();
}

/**
 * @brief Main Task in OTA Process
 * 
 * This task are resposible to update the ESP8266.
 * // TODO: Add capility to read a JSON with bin to update and spiffs
 * // TODO: Change the way from HTTPS for a TCP Server
 * 
 */
void e12aio_ota_task(void *args)
{
    xTaskCreate(e12aio_ota_watchdog, "ota_watchdog", 2048, NULL, 5, NULL);
    e12aio_wifi_sta_wait_connect(TAG);
    e12aio_wifi_ap_wait_deactive(TAG);
    char *l_cert_pem = (char *)server_certs_start;
    char l_ota_url[E12AIO_OTA_URL_SIZE];
    e12aio_ota_file_read_url_firmware((char *)&l_ota_url, E12AIO_OTA_URL_SIZE);
    ESP_LOGI(TAG, "Starting OTA Update: %s", l_ota_url);
    esp_http_client_config_t config = {
        .url = l_ota_url,
        .cert_pem = l_cert_pem,
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

/**
 * @brief Function resposible to detect what stack to run (OTA or Normal)
 */
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

size_t e12aio_ota_file_write_url_firmware(char *buffer, size_t sz)
{
    return e12aio_spiffs_write(ota_file_url_firmware, buffer, sz);
}

size_t e12aio_ota_file_write_url_spiffs(char *buffer, size_t sz)
{
    return e12aio_spiffs_write(ota_file_url_spiffs, buffer, sz);
}

size_t e12aio_ota_file_read_url_firmware(char *buffer, size_t sz)
{
    return e12aio_spiffs_read(ota_file_url_firmware, buffer, sz);
}

size_t e12aio_ota_file_read_url_spiffs(char *buffer, size_t sz)
{
    return e12aio_spiffs_read(ota_file_url_spiffs, buffer, sz);
}

/**
 * @brief Before invoke this function, you must write the URL usign `e12aio_ota_write_url`.
 */
void e12aio_ota_start()
{
    strncpy(e12aio_config_get()->ota.version, E12AIO_VERSION, E12AIO_OTA_VERSION_SIZE);
    e12aio_config_get()->ota.state = E12AIO_OTA_SCHEDULED;
    e12aio_config_save();
    vTaskDelay(100 / portTICK_PERIOD_MS);
    esp_restart();
}

#endif