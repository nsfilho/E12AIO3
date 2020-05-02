# Home Automation

The main goals of this project is provide a simple way to do some home automation [based on a cheap and
small board](https://easyeda.com/DIY-Maker-BR/placa-4-reles-esp12f)

Project licensed under: GPLv3

## Firmware

Main features:

-   Home assistant auto discovery;
-   Networking detection system (auto soft-ap & station);
-   MQTT Keep Alive;
-   Web Configuration (mobile-first) - _under development_
-   Physical GPIO: I2C Oled, Binary Sensor, Button Sensor (with debouncing) - _under development_

## FAQ

### Configure Home Assistant Auto Discovery

You need to add a few configurations to your home assistant existing MQTT configuration, example:

```yaml
mqtt:
    broker: 127.0.0.1
    port: 1883
    client_id: homeassistant
    keepalive: 300
    username: !secret mqtt_username
    password: !secret mqtt_password
    protocol: 3.1
    discovery: true
    discovery_prefix: home
```

Where `home` is your `baseTopic`.

### MQTT Topics

Topics for command your relays:

```
<baseTopic>/switch/e12aio3_<id>/relay<1-3>/set
```

Topics for state your relays:

```
<baseTopic>/switch/e12aio3_<id>/relay<1-3>
```

Your sensor topics:

```
<baseTopic>/sensor/e12aio3_<id>/uptime
<baseTopic>/sensor/e12aio3_<id>/model
<baseTopic>/sensor/e12aio3_<id>/build
<baseTopic>/sensor/e12aio3_<id>/ipaddr
```

Action topics:

```
GET: <baseTopic>/action/e12aio3_<id>/config/get
SET: <baseTopic>/action/e12aio3_<id>/config/set
RES: <baseTopic>/action/e12aio3_<id>/config
```

```
GET: <baseTopic>/action/e12aio3_<id>/scan/get
RES: <baseTopic>/action/e12aio3_<id>/scan
```

### How to firmware?

Use [ESP8266-RTOS IDF Like](https://docs.espressif.com/projects/esp8266-rtos-sdk/en/release-v3.3/index.html#)

### How to configure?

Use `make menuconfig`

### How to flash?

Use `make flash`

### Action Topics

It is a special kind of topic for you send (or ask for something) and you take an answer.

### JSON Example

```json
{
    "wifi": {
        "ssid": "myNetwork",
        "password": "mySecret"
    },
    "mqtt": {
        "url": "mqtt://user:password@host:port",
        "topic": "home/"
    },
    "relay": [0, 0, 0]
}
```
