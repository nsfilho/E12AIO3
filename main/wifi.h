#pragma once

void wifi_init();
void wifi_check_available_networks();
void wifi_loop_task(void *pvParameters);
