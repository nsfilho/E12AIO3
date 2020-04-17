#pragma once

void mqtt_init();
void mqtt_loop_task(void *arg);
void mqtt_keepalive_task(void *arg);
