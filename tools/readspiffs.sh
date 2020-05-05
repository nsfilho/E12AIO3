#!/bin/bash

# Retrieve from ESP8266 if is needed
if [ ! -f spiffs.bin ] ; then
    esptool.py --port $ESPPORT read_flash 884736 163840 spiffs.bin
fi

# Remove old files
if [ -d spiffs ] ; then
    rm -rf spiffs
fi

# Extract the content
mkdir spiffs
mkspiffs/mkspiffs -p 256 -u spiffs spiffs.bin
