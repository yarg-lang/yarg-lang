#!/bin/bash

BUILD_ERROR=0

./tools/build-host.sh || BUILD_ERROR=1

if which picotool > /dev/null
then
    ./tools/build-target-pico.sh || BUILD_ERROR=1
else
    echo "picotool not found, skipping build for Raspberry Pi Pico"
fi

if which xcodebuild > /dev/null && xcodebuild -help &> /dev/null
then
    ./tools/build-xcode.sh || BUILD_ERROR=1
else
    echo "xcodebuild not configured, skipping build for Xcode"
fi

if which vsce > /dev/null
then
    ./tools/build-vscode-yarg.sh || BUILD_ERROR=1
else
    echo "vsce not found, skipping build for VS Code extension"
fi

./tools/test.sh || BUILD_ERROR=1

./tools/build-release-images.sh || BUILD_ERROR=1
./tools/build-release-tarball.sh || BUILD_ERROR=1
./tools/build-conway.sh || BUILD_ERROR=1

exit $BUILD_ERROR