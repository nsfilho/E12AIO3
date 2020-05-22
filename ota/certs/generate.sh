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
cd $WORKDIR

if [ "x$1" == "x" ] ; then
    echo "You need pass as argument your machine ip address"
    exit 0
fi

rm -f openssl.conf

cat << _EOF > openssl.cnf
[ req ]
prompt = no
distinguished_name = req_distinguished_name

[ req_distinguished_name ]
C = BR
ST = Sao Paulo
L = Bauru
O = E01-AIO Automacao Ltda
OU = Research and Development
CN = $1
emailAddress = nelio@e01aio.com.br
_EOF

openssl req -nodes -x509 -newkey rsa:2048 \
 -keyout ca_key.pem \
 -out ca_cert.pem -days 365 \
 -config openssl.cnf