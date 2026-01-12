#!/bin/bash

BUILD_ERROR=0

./tools/build-host.sh || BUILD_ERROR=1
./tools/build-target-pico.sh || BUILD_ERROR=1

./tools/build-specimen.sh || BUILD_ERROR=1

if which vsce > /dev/null
then
    ./tools/build-vscode-yarg.sh || BUILD_ERROR=1
fi

if which xcodebuild > /dev/null
then
    ./tools/build-xcode.sh || BUILD_ERROR=1
fi

exit $BUILD_ERROR