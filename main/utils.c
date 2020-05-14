#include <esp_timer.h>
#include <esp_system.h>
#include "utils.h"

size_t e12aio_utils_uptime(char *buffer, size_t sz)
{
    uint32_t sec = esp_timer_get_time() / 1000ULL / 1000ULL;
    uint16_t min = sec / 60;
    uint16_t hour = min / 60;
    return snprintf(buffer, sz, "%02d:%02d:%02d", hour, min % 60, sec % 60);
}