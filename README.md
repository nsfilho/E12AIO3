# Home Automation

The main goals of this project is provide a simple way to do some home automation [based on a cheap and
small board](https://easyeda.com/DIY-Maker-BR/placa-4-reles-esp12f)

## Firmware

Main features:

-   Home assistant compatible;
-   Networking detection system (auto soft-ap & station);
-   MQTT Keep Alive;
-   Web Configuration (mobile-first)

## FAQ

### How to firmware?

Use [ESP8266-RTOS IDF Like](https://docs.espressif.com/projects/esp8266-rtos-sdk/en/release-v3.3/index.html#)

### How to configure?

Use `make menuconfig`

### How to flash?

Use `make flash`
