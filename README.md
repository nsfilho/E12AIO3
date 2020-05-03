# Home Automation

The main goal of this project is provide a simple way to do some home automation [based on a cheap and
small board](https://easyeda.com/DIY-Maker-BR/placa-4-reles-esp12f)

Project licensed under: GPLv3 for board and firmware.

## Board

The board comes with all components soldered from [JLCPCB](https://jlcpcb.com), when you bought with SMT Assembly Services. You need only
solder the relays (3x units), KREs (3x 3 vias, 1x 2 vias) and a ESP12 (E or F). Its super easy.

![Board photo](docs/board.png)

Files links to order your board:

-   [Gerber File](docs/Gerber_20200130210739.zip)
-   [Bill of Material](docs/BOM_20200130210745.csv)
-   [Pick and Place](docs/PickAndPlace_20200130210754.csv)

Characteristcs from this board:

-   Source Voltage: 5v
-   Current (maximum usage, without extensions): 540mA
-   Extension headers: I2C, Analog Input, Additionals GPIO
-   Small size: 70mm x 49mm
-   Manual reset and flash buttons
-   Flash friendly (as describe in FAQ)
-   3 relays (because it is a cold component -- you can use inside your wall conduit box).

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

-   GET: command to get informations
-   SET: command to set action
-   RES: the topic where you will receive the result from GET

```
GET: <baseTopic>/action/e12aio3_<id>/config/get, PAYLOAD: json
SET: <baseTopic>/action/e12aio3_<id>/config/set, PAYLOAD: { .... json bellow .... }
RES: <baseTopic>/action/e12aio3_<id>/config, PAYLOAD: { ... json bellow... }
```

```
SET: <baseTopic>/action/e12aio3_<id>/restart/set, PAYLOAD: yes
```

```
GET: <baseTopic>/action/e12aio3_<id>/scan/get, PAYLOAD: json
RES: <baseTopic>/action/e12aio3_<id>/scan, PAYLOAD: { ... json under development ... }
```

### How to firmware?

This firmware was built in [ESP8266-RTOS IDF Like](https://docs.espressif.com/projects/esp8266-rtos-sdk/en/release-v3.3/index.html#).

You can use a ESP8266 Programmer (and avoid to solder serial headers) or via serial (TX, RX, GND).

If you have a FTDI / CP2104 or CH340 with DTR and CTS, the board is flash friendly (auto enter in programmer mode and restart after flash).

### How to configure default values from firmware?

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
