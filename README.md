# Home Automation

![ESP8266-RTOS-SDK Build](https://github.com/nsfilho/E12AIO3/workflows/ESP8266-RTOS-SDK%20Build/badge.svg)

The main goal of this project is provide a simple way to do some home automation [based on a cheap and
small board](https://easyeda.com/DIY-Maker-BR/placa-4-reles-esp12f)

Project licensed under: GPLv3 for board and firmware.

## License

```txt

 E12AIO3 Firmware
 Copyright (C) 2020 E01-AIO Automação Ltda.

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 Author: Nelio Santos <nsfilho@icloud.com>
 Repository: https://github.com/nsfilho/E12AIO3

```

## Board

The board comes with all components soldered from [JLCPCB](https://jlcpcb.com), when you bought with SMT Assembly Services. You need only
solder the relays (3x units), KREs (3x 3 vias, 1x 2 vias) and a ESP12 (E or F). Its super easy.

![Board photo](docs/board.png)

Files links to order your board:

-   [Gerber File](docs/Gerber_20200516174000.zip)
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

If you want to deep dive, you can see the [Schematic File](docs/Schematic_2020-05-03_09-55-35.pdf)

## Web Interface

You can see here a sample screenshot of a HTML5 (and responsive) interface.

![Web UI - Dashboard View](docs/WebUI.png)

If you click in Relay Status Port, you can toogle from ON / OFF.

## Firmware

Main features:

-   ✓ 🏠 Home assistant auto discovery;
-   ✓ 📡 Networking detection system (auto soft-ap & station);
-   ✓ 🎛 MQTT Keep Alive;
-   ✓ 🛠 Web Configuration (mobile-first)
-   ✓ 🕹 Web API
-   ✓ 🎉 OTA (Over-the-air) Firmware Update - _under development_
-   ⚡︎ Physical GPIO: I2C Oled, Binary Sensor, Button Sensor (with debouncing) - _backlog_

## Flash your firmware

**Method 1:** Exists two different ways to do that. First using the Espress IDF (as tooling), you can:

```sh
make all
make erase_flash upload_spiffs flash
```

**Method 2:** Using `esptool.py`

For install `esptool.py` you can use [PlatformIO CLI](https://docs.platformio.org/en/latest/core/installation.html).

> Important: don't forget to put `PlatformIO` binary folder in your path.

After install, you need prepare the environment, as showed bellow:

```sh
pio platform install espressif8266 --with-all-packages
ls ~/.platformio/packages/tool-esptoolpy/
```

Now you are ready to flash, first download the files in [release](https://github.com/nsfilho/E12AIO3/releases) section, if you download in `zip` format, extract it.

You need replace the following information, in the command bellow:

1. **/dev/cu.SLAB_USBtoUART** for your serial-usb port;

```sh
python ~/.platformio/packages/tool-esptoolpy/esptool.py --port /dev/cu.SLAB_USBtoUART erase_flash
python ~/.platformio/packages/tool-esptoolpy/esptool.py --port /dev/cu.SLAB_USBtoUART write_flash 0x310000 spiffs.bin
python ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp8266 --port /dev/cu.SLAB_USBtoUART --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size 4MB 0xd000 ota_data_initial.bin 0x0000 bootloader.bin 0x10000 e12aio3.bin 0x8000 partitions.bin
```

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
<baseTopic>/sensor/e12aio3_<id>/freemem
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