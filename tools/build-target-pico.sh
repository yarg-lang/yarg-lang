#!/bin/bash
BUILD_ERROR=0

pushd hostyarg
go build -o ../bin/yarg ./cmd/yarg || BUILD_ERROR=1
popd

pushd cyarg
cmake --preset target-pico . || BUILD_ERROR=1
cmake --build build/target-pico || BUILD_ERROR=1

cmake --preset target-pico-debug . || BUILD_ERROR=1
cmake --build build/target-pico-debug || BUILD_ERROR=1
popd

mkdir -p build
cp cyarg/build/target-pico/cyarg.uf2 build/yarg-lang.uf2 || BUILD_ERROR=1

./bin/yarg format -image build/yarg-lang.uf2
./tools/add-yarg-stdlib.sh build/yarg-lang.uf2

cp cyarg/build/target-pico-debug/cyarg.uf2 build/yarg-lang-debug.uf2 || BUILD_ERROR=1

./bin/yarg format -image build/yarg-lang-debug.uf2
./tools/add-yarg-stdlib.sh build/yarg-lang-debug.uf2

./tools/build-specimen.sh
./tools/build-test-hardware.sh

exit $BUILD_ERROR