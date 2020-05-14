#pragma once
#include <freertos/FreeRTOS.h>

#define E12AIO_UTILS_UPTIME_SIZE 25

size_t e12aio_utils_uptime(char *buffer, size_t sz);