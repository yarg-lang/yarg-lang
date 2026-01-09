#!/bin/bash

VSCODEEXTENSION_VERSION=0.3.0

mkdir -p bin
cp tools/pico-reset bin/

pushd hostyarg
go build .
popd

cp hostyarg/hostyarg bin/

pushd cyarg
cmake --preset target-pico .
cmake --build build/target-pico

cmake --preset target-pico-debug .
cmake --build build/target-pico-debug

cmake --preset host
cmake --build build/host

cmake --preset host-test
cmake --build build/host-test
popd

cp cyarg/build/host-test/cyarg bin/

mkdir -p build
cp cyarg/build/target-pico/cyarg.uf2 build/yarg-lang.uf2

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

cp cyarg/build/target-pico-debug/cyarg.uf2 build/yarg-lang-debug.uf2

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
    vsce package
    mv yarg-lang-$VSCODEEXTENSION_VERSION.vsix ../build/
    popd
fi

if which xcodebuild > /dev/null
then
    pushd cyarg
    cmake --preset xcode-host
    cmake --build build/xcode-host
    popd
fi
