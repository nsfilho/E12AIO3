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

if [ ! -d $WORKDIR/content/pages ] ; then
    echo You have no access to the default webpage source code.
    echo
    echo But you have two options:
    echo 1. Use spiffs.bin as-is 
    echo 2. Create your web config page
    echo
    exit 1
fi

if [ ! -x $WORKDIR/mkspiffs/mkspiffs ] ; then
    echo You need prepare mkspiffs first. Please build the software properly.
    exit 1
fi

if [ -f $WORKDIR/spiffs.bin ] ; then
    rm -f $WORKDIR/spiffs.bin
fi

echo "Workdir: $WORKDIR"
echo "Building files..."
cd $WORKDIR/content && yarn start

echo "Build image..."
$WORKDIR/mkspiffs/mkspiffs \
     -d 5 -s 307200 -p 256 \
    -c $WORKDIR/content/dist \
    $WORKDIR/spiffs.bin