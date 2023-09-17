#!/bin/bash
ARM_TOOLCHAIN = "<path/to/toolchain>"
NRF5_SDK = "<path/to/sdk>"

rm -rf build

mkdir build
cd build
cmake "-DARM_NONE_EABI_TOOLCHAIN_PATH=$ARM_TOOLCHAIN" "-DNRF5_SDK_PATH=$NRF5_SDK" -DBUILD_DFU=1 -DBUILD_RESOURCES=0