#!/bin/bash

mkdir bin
cp tools/pico-reset bin/

pushd hostyarg
go build .
popd

cp hostyarg/hostyarg bin/

pushd cyarg
cmake --preset pico .
cmake --build build/pico

cmake --preset pico-debug .
cmake --build build/pico-debug

cmake --preset host
cmake --build build/host

cmake --preset hosttest
cmake --build build/hosttest
popd

cp cyarg/build/hosttest/cyarg bin/

mkdir build
cp cyarg/build/pico/cyarg.uf2 build/yarg-lang.uf2

./bin/hostyarg format -fs build/yarg-lang.uf2

pushd yarg/specimen
../../bin/hostyarg addfile -fs ../../build/yarg-lang.uf2 -add machine.ya
../../bin/hostyarg addfile -fs ../../build/yarg-lang.uf2 -add gpio.ya
../../bin/hostyarg addfile -fs ../../build/yarg-lang.uf2 -add irq.ya
popd

pushd vscode-yarg
vsce package
mv yarg-lang-0.1.0.vsix ../build/
popd
