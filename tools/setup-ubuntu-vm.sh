#!/bin/bash

# a specimen script to get a vanilla Ubuntu 25.10 VM ready for building
# You may prefer an alternative approach.
# Note that the C++ in cyarg/test-system requires GCC 14 or later. 15 appears
# to be the default in Ubuntu 25.10.

sudo apt -y install build-essential
sudo apt -y install golang cmake ninja-build

if [ "$1" = "pico" ]; then
    echo "Setting up for Pico development"

    mkdir -p pico-tooling
    pushd pico-tooling

    ARM_CROSS_COMPILER_VERSION="14.2.rel1"
    ARM_CROSS_COMPILER_HOST="$(arch)"
    ARM_TOOLCHAIN_FILENAME="arm-gnu-toolchain-${ARM_CROSS_COMPILER_VERSION}-${ARM_CROSS_COMPILER_HOST}-arm-none-eabi.tar.xz"
    
    wget https://developer.arm.com/-/media/Files/downloads/gnu/${ARM_CROSS_COMPILER_VERSION}/binrel/$ARM_TOOLCHAIN_FILENAME
    tar xf $ARM_TOOLCHAIN_FILENAME

    TOOLCHAIN_DIR="$(pwd)/arm-gnu-toolchain-${ARM_CROSS_COMPILER_VERSION}-${ARM_CROSS_COMPILER_HOST}-arm-none-eabi"
    export PATH="$TOOLCHAIN_DIR/bin:$PATH"
    echo "export PATH=\"$TOOLCHAIN_DIR/bin:\$PATH\"" >> ~/.bashrc

    git clone --recurse-submodules https://github.com/raspberrypi/pico-sdk.git \
        --branch 2.2.0 --single-branch --depth 1

    PICO_SDK_DIR="$(pwd)/pico-sdk"
    export PICO_SDK_PATH="$PICO_SDK_DIR"
    echo "export PICO_SDK_PATH=\"$PICO_SDK_DIR\"" >> ~/.bashrc

    git clone https://github.com/raspberrypi/picotool.git \
              --branch 2.2.0-a4 --single-branch --depth 1
    cd picotool
    mkdir build
    cd build
    cmake ..
    cmake --build . --parallel $(nproc)

    sudo cmake --install .
fi