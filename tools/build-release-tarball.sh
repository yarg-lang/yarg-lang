#!/bin/bash

RELEASE_VERSION=$(git describe --tags --dirty=M --always)
VSCODEEXTENSION_VERSION=0.3.0
TARGETUF2=yarg-lang-pico-$RELEASE_VERSION.uf2
RELEASE_TARBALL=yarg-lang-pico-$RELEASE_VERSION.tgz

if [ -e release/$RELEASE_TARBALL ]
then
    rm release/$RELEASE_TARBALL
fi

if [ -e build/release ]
then
    rm -Rf build/release
fi

mkdir -p build/release
mkdir -p build/release/extras

cp build/yarg-lang.uf2 build/release/$TARGETUF2
cp docs/release_README build/release/README
cp build/yarg-lang-$VSCODEEXTENSION_VERSION.vsix build/release/extras/

if [ ! -e release ]
then
    mkdir release
fi
tar -cvzf release/yarg-lang-pico-$RELEASE_VERSION.tgz -C ./build/release .
