#!/bin/bash

VSCODEEXTENSION_VERSION=0.3.0
BUILD_ERROR=0

mkdir -p bin
cp tools/pico-reset bin/

pushd hostyarg
go build .
popd

cp hostyarg/hostyarg bin/

pushd cyarg
cmake --preset target-pico . || BUILD_ERROR=1
cmake --build build/target-pico || BUILD_ERROR=1

cmake --preset target-pico-debug . || BUILD_ERROR=1
cmake --build build/target-pico-debug || BUILD_ERROR=1

cmake --preset host || BUILD_ERROR=1
cmake --build build/host || BUILD_ERROR=1

cmake --preset host-test || BUILD_ERROR=1
cmake --build build/host-test || BUILD_ERROR=1
popd

cp cyarg/build/host-test/cyarg bin/

mkdir -p build
cp cyarg/build/target-pico/cyarg.uf2 build/yarg-lang.uf2 || BUILD_ERROR=1

./bin/hostyarg format -fs build/yarg-lang.uf2

pushd yarg/specimen
../../bin/hostyarg addfile -fs ../../build/yarg-lang.uf2 -add machine.ya
../../bin/hostyarg addfile -fs ../../build/yarg-lang.uf2 -add gpio.ya
../../bin/hostyarg addfile -fs ../../build/yarg-lang.uf2 -add irq.ya
../../bin/hostyarg addfile -fs ../../build/yarg-lang.uf2 -add timer.ya
../../bin/hostyarg addfile -fs ../../build/yarg-lang.uf2 -add uart.ya
../../bin/hostyarg addfile -fs ../../build/yarg-lang.uf2 -add reset.ya
../../bin/hostyarg addfile -fs ../../build/yarg-lang.uf2 -add clock.ya
../../bin/hostyarg addfile -fs ../../build/yarg-lang.uf2 -add rosc.ya
../../bin/hostyarg addfile -fs ../../build/yarg-lang.uf2 -add pio.ya
../../bin/hostyarg addfile -fs ../../build/yarg-lang.uf2 -add pio-instructions.ya
../../bin/hostyarg addfile -fs ../../build/yarg-lang.uf2 -add dma.ya
../../bin/hostyarg addfile -fs ../../build/yarg-lang.uf2 -add ws2812.ya
../../bin/hostyarg addfile -fs ../../build/yarg-lang.uf2 -add apa102.ya
popd

cp cyarg/build/target-pico-debug/cyarg.uf2 build/yarg-lang-debug.uf2 || BUILD_ERROR=1

./bin/hostyarg format -fs build/yarg-lang-debug.uf2

pushd yarg/specimen
../../bin/hostyarg addfile -fs ../../build/yarg-lang-debug.uf2 -add machine.ya
../../bin/hostyarg addfile -fs ../../build/yarg-lang-debug.uf2 -add gpio.ya
../../bin/hostyarg addfile -fs ../../build/yarg-lang-debug.uf2 -add irq.ya
../../bin/hostyarg addfile -fs ../../build/yarg-lang-debug.uf2 -add timer.ya
../../bin/hostyarg addfile -fs ../../build/yarg-lang-debug.uf2 -add uart.ya
../../bin/hostyarg addfile -fs ../../build/yarg-lang-debug.uf2 -add reset.ya
../../bin/hostyarg addfile -fs ../../build/yarg-lang-debug.uf2 -add clock.ya
../../bin/hostyarg addfile -fs ../../build/yarg-lang-debug.uf2 -add pio.ya
../../bin/hostyarg addfile -fs ../../build/yarg-lang-debug.uf2 -add pio-instructions.ya
../../bin/hostyarg addfile -fs ../../build/yarg-lang-debug.uf2 -add dma.ya
../../bin/hostyarg addfile -fs ../../build/yarg-lang-debug.uf2 -add ws2812.ya
popd

./tools/build-specimen.sh

if which vsce > /dev/null
then
    pushd vscode-yarg
    vsce package || BUILD_ERROR=1
    mv yarg-lang-$VSCODEEXTENSION_VERSION.vsix ../build/
    popd
fi

if which xcodebuild > /dev/null
then
    pushd cyarg
    cmake --preset xcode-host || BUILD_ERROR=1
    cmake --build build/xcode-host || BUILD_ERROR=1
    popd
fi

exit $BUILD_ERROR