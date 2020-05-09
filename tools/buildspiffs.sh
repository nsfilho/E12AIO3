#!/bin/bash
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
     -d 5 -s 163840 -p 256 \
    -c $WORKDIR/content/dist \
    $WORKDIR/spiffs.bin