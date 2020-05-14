#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_log.h>

#ifdef CONFIG_COMPONENT_DEBUG
static const char *TAG = "debug.c";

/**
 * @brief Start the debug task
 * 
 * Print basic informations as heap size.
 */
void e12aio_debug_task(void *arg)
{
    for (;;)
    {
        ESP_LOGW(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
        vTaskDelay(CONFIG_DEBUG_INFO_DELAY / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}
#endif

/**
 * @brief Initialize debug system
 */
void e12aio_debug_init()
{
#ifdef CONFIG_COMPONENT_DEBUG
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    xTaskCreate(e12aio_debug_task, "debug_task", 2048, NULL, 1, NULL);
#endif
}
