#!/bin/bash

if [[ "$buildtype" = "" ]]; then
    buildtype="debug"
fi

mkdir -p build
meson setup build --buildtype="$buildtype" $MESONARG
cd build
ninja
cd ..