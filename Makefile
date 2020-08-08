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
PROJECT_NAME := e12aio3

# exclude `mqtt` component in ESP8266 RTOS SDK, which has been lagged behind,
# and broken. use `esp-mqtt` under components directory
ifeq ($(PROJECT_TARGET), esp8266)
EXCLUDE_COMPONENTS = mqtt
else
EXCLUDE_COMPONENTS = esp-mqtt
endif

include $(IDF_PATH)/make/project.mk

allcerts:
	cat ota/certs/*_cert.pem > ota/certs/allcerts.pem

build_spiffs:
	cd tools && ./buildspiffs.sh

upload_spiffs:
	cd tools && ./writespiffs.sh
