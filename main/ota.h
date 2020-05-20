#pragma once
#include <esp_http_client.h>

esp_err_t e12aio_ota_http_event_handler(esp_http_client_event_t *evt);
bool e12aio_ota_init();
void e12aio_ota_task(void *args);
void e12aio_ota_start(const char *url);