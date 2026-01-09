#!/bin/bash

BUILD_ERROR=0

pushd hostyarg
go build -o ../bin/hostyarg .
popd


pushd cyarg
cmake --preset host || BUILD_ERROR=1
cmake --build build/host || BUILD_ERROR=1

cmake --preset host-test || BUILD_ERROR=1
cmake --build build/host-test || BUILD_ERROR=1
popd

cp cyarg/build/host-test/cyarg bin/

exit $BUILD_ERROR