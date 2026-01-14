#!/bin/bash

RELEASE_VERSION=$(git describe --tags --dirty=M --always)
VSCODEEXTENSION_VERSION=0.3.0
TARGETUF2=build/yarg-lang-pico-$RELEASE_VERSION.uf2

if [ -e release/yarg-lang-pico-$RELEASE_VERSION.tgz ]
then
    rm release/yarg-lang-pico-$RELEASE_VERSION.tgz
fi

if [ -e $TARGETUF2 ]
then
    rm $TARGETUF2
fi

if [ -e build/release ]
then
    rm -Rf build/release
fi

mkdir -p build/release
mkdir -p build/release/extras

pushd hostyarg
go build -o ../bin/hostyarg .
popd

pushd cyarg
cmake --preset target-pico . || BUILD_ERROR=1
cmake --build build/target-pico || BUILD_ERROR=1
popd

./tools/build-vscode-yarg.sh || BUILD_ERROR=1
./tools/build-release-images.sh || BUILD_ERROR=1
./tools/build-release-tarball.sh || BUILD_ERROR=1

exit $BUILD_ERROR
