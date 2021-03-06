#!/bin/bash
#
# E12AIO3 Firmware
# Copyright (C) 2020 E01-AIO Automação Ltda.
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
# 
# Author: Nelio Santos <nsfilho@icloud.com>
# Repository: https://github.com/nsfilho/E12AIO3
#

WORKDIR="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

cd $WORKDIR/..
SPIFFS_START=`make partition_table | grep spiffs | cut -d ',' -f 4`
#SPIFFS_SIZE=`make partition_table | grep spiffs | cut -d ',' -f 5`
echo "Spiffs Start: $SPIFFS_START"
cd $WORKDIR

# Retrieve from ESP8266 if is needed
if [ ! -f spiffs.bin ] ; then
    esptool.py --port $ESPPORT read_flash ${SPIFFS_START} 512000 spiffs.bin
fi

# Remove old files
if [ -d spiffs ] ; then
    rm -rf spiffs
fi

# Extract the content
mkdir spiffs
mkspiffs/mkspiffs -p 256 -u spiffs spiffs.bin
