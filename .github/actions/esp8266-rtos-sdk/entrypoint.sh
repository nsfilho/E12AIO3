#!/bin/sh

if [ ! -f ota/certs/ca_cert.pem ] ; then
    sh ota/certs/generate.sh
fi

make defconfig all