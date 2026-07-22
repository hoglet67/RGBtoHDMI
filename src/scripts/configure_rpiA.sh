#!/bin/sh

cmake -G "Unix Makefiles" "$@" -DCMAKE_TOOLCHAIN_FILE=../toolchain-arm-none-eabi-rpiA.cmake ../
