#pragma once

#define D_WIFI_CONNECTED BIT0
#define D_WIFI_SCANNING BIT1

void wifi_init();
void wifi_check_available_networks();
void wifi_loop_task(void *pvParameters);
void config_wait_loaded();