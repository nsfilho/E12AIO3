#!/bin/bash

if [ ! -f spiffs.bin ] ; then
    esptool.py --port $ESPPORT read_flash 884736 163840  myspiffs.bin
fi

if [ -d spiffs ] ; then
    rm -rf spiffs
fi
mkdir spiffs
mkspiffs/mkspiffs -d 5 -p 256 -u myspiffs spiffs.bin

cat myspiffs/config.json