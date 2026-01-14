#!/bin/bash

BUILD_ERROR=0
TARGETUF2=yarg-lang.uf2

if [ -e build/$TARGETUF2 ]
then
    rm build/$TARGETUF2
fi

mkdir -p build
cp cyarg/build/target-pico/cyarg.uf2 build/$TARGETUF2 || BUILD_ERROR=1

./bin/hostyarg format -fs build/$TARGETUF2
./tools/add-yarg-stdlib.sh build/$TARGETUF2 || BUILD_ERROR=1

# for now fill the release with specimens
./tools/add-specimen.sh build/$TARGETUF2 || BUILD_ERROR=1

exit $BUILD_ERROR
