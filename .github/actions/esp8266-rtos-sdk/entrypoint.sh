#!/bin/bash

if [ ! -f ota/certs/ca_cert.pem ] ; then
    bash ota/certs/generate.sh
fi

make defconfig all