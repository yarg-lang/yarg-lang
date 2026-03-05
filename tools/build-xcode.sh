#!/bin/bash

BUILD_ERROR=0

pushd hostyarg
go build -o ../bin/yarg ./cmd/yarg || BUILD_ERROR=1
popd

pushd cyarg
cmake --preset xcode-host || BUILD_ERROR=1
cmake --build build/xcode-host || BUILD_ERROR=1
popd

./bin/yarg runtests -tests "test/hostyarg" -interpreter "cyarg/build/xcode-host/Debug/cyarg" || BUILD_ERROR=1

exit $BUILD_ERROR