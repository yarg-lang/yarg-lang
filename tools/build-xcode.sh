#!/bin/bash

BUILD_ERROR=0

pushd hostyarg
go build -o ../bin/hostyarg .
popd

pushd cyarg
cmake --preset xcode-host || BUILD_ERROR=1
cmake --build build/xcode-host || BUILD_ERROR=1
popd

./bin/hostyarg runtests -tests "yarg/test" -interpreter "cyarg/build/xcode-host/Debug/cyarg" || BUILD_ERROR=1

exit $BUILD_ERROR