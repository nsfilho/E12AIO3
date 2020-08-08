# Home Automation

![ESP8266-RTOS-SDK Build](https://github.com/nsfilho/E12AIO3/workflows/ESP8266-RTOS-SDK%20Build/badge.svg)
![Release](https://github.com/nsfilho/E12AIO3/workflows/Release/badge.svg?event=release)

The main goal of this project is provide a simple way to do some 🏠 automation [based on a cheap and
small board](https://easyeda.com/DIY-Maker-BR/placa-4-reles-esp12f).

Project licensed under: GPLv3 (board and firmware).

## Board

The board comes with most of all components soldered from [JLCPCB](https://jlcpcb.com), when you bought with SMT Assembly Services. You need only solder the relays (3x units), KREs (3x 3 vias, 1x 2 vias) and a ESP12 (E or F). Its super easy.

We provide (in portuguese by now) a [youtube video](https://youtu.be/1pT6UaQUK3s) describing this process.

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

You can see here a sample screenshot of our HTML5 (and responsive) interface 😍.

![Web UI - Dashboard View](docs/WebUI.png)

If you click at relay status port, you can toggle their state (ON / OFF). Any kind of idea are wellcome. If you are `web design`, `web developer` or at least a entusiast and want to contribute with that, please e-mail me.

## Firmware 2.3.0

Main features:

-   ✓ 🏠 Home assistant auto discovery;
-   ✓ 📡 Networking detection system (auto soft-ap & station);
-   ✓ 🎛 MQTT Keep Alive;
-   ✓ 🦾 MQTT Library updated version (handle many messages in a short time);
-   ✓ 🛠 Web Configuration (mobile-first)
-   ✓ 🕹 Web API
-   ✓ 🎉 OTA (Over-the-air) Firmware Update
-   ⚡︎ Physical GPIO: I2C Oled, Binary Sensor, Button Sensor (with debouncing) - _backlog_

## OTA Firmware URL

You can update your board using a github url: [https://nsfilho.github.io/E12AIO3/e12aio3.bin](https://nsfilho.github.io/E12AIO3/e12aio3.bin). When we release a new version, automatically (by github actions) comes available in this url.

## Flash Firmware

You can find many ways to flash your board. We show bellow 2 distinct methods. The first, using a more sophisticated environment (like a developer) and the second a little more `easy`.

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
2. **0x310000** address - could change because we are in a revision process of `partitions.csv`;

```sh
python ~/.platformio/packages/tool-esptoolpy/esptool.py --port /dev/cu.SLAB_USBtoUART erase_flash
python ~/.platformio/packages/tool-esptoolpy/esptool.py --port /dev/cu.SLAB_USBtoUART write_flash 0x310000 spiffs.bin
python ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp8266 --port /dev/cu.SLAB_USBtoUART --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size 4MB 0xd000 ota_data_initial.bin 0x0000 bootloader.bin 0x10000 e12aio3.bin 0x8000 partitions.bin
```

## FAQ

### Configure Home Assistant Auto Discovery

You need to add a few configurations to your home assistant existing MQTT configuration.

1. `discovery: true` to enable auto-discovery via MQTT;
2. `discovery_prefix:` where the boards will send the messages for config;

Sample:

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

> `home` is your `baseTopic`.

### MQTT Topics

Topics for command your relays, where payloads is `ON` or `OFF`:

```
<baseTopic>/switch/e12aio3_<id>/relay1/set
<baseTopic>/switch/e12aio3_<id>/relay2/set
<baseTopic>/switch/e12aio3_<id>/relay3/set
```

State topics for your relays:

```
<baseTopic>/switch/e12aio3_<id>/relay1
<baseTopic>/switch/e12aio3_<id>/relay2
<baseTopic>/switch/e12aio3_<id>/relay3
```

Sensor topics:

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

> In version 2.2.x this functionallity (config/get or config/set) are disabled.

```
SET: <baseTopic>/action/e12aio3_<id>/restart/set, PAYLOAD: yes
```

```
GET: <baseTopic>/action/e12aio3_<id>/scan/get, PAYLOAD: json
RES: <baseTopic>/action/e12aio3_<id>/scan, PAYLOAD: { ... json under development ... }
```

### How to build this firmware?

This firmware was built in [ESP8266-RTOS IDF Like](https://docs.espressif.com/projects/esp8266-rtos-sdk/en/release-v3.3/index.html#).

You can use a ESP8266 Programmer (and avoid to solder serial headers) or via serial (TX, RX, GND). Because you update the firmware via OTA 🙏.

If you have a FTDI / CP2104 or CH340 with DTR and CTS, the board is flash friendly (auto enter in programmer mode and restart after each step for flashing).

### How to configure default values from firmware?

Use `make menuconfig`

### How to flash?

Use `make erase_flash upload_spiffs flash`

### Can I host OTA Server Locally?

Yes. You can use the project inside `ota` folder to do that. You need generate certificates (self-signed) first, using `ota/certs/generate.sh <ip>`

> **Important**: You must specify the IP address. Because you need to use the same IP in upload to check SSL certificate. If you have any doubt about that, please let us know.

### Why HTMLs and others are packaged?

The process to build the `spiffs.bin` it's a little complex to explain. We use a lot of files, and `GULP` to post-process all of then. To help you understand, GULP do:

1. generate a dynamic icon font, using only the SVGs is used;
2. merge all css files in a unique file;
3. minify htmls, css and images;
4. re-structure folders from a `organized tree` to `dist tree`;

All spare files (to build the `spiffs.bin`) use much more than **1.5mb**, and it is compressed in `less than 150kb`. This technology is part of a `private` repository. But if you **really want** to contribute, please let me know.

To develop that part of software in a more easy way, we created a `httpd server`, what provide to the browser, all files in an exactly same way of ESP8266. This httpd `watch file changes` in a directory and trigger `gulp` when something is changed.

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
