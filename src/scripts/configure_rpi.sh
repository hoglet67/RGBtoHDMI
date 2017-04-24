#!/bin/sh

cmake -G "CodeBlocks - Unix Makefiles" "$*" -DCMAKE_TOOLCHAIN_FILE=../toolchain-arm-none-eabi-rpi.cmake ../
