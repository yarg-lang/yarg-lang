#!/bin/bash

HOSTYARG="bin/yarg"
TARGETUF2="build/test-hardware-compiled.uf2"


BUILD_DIR=build/test-hardware-compiled

if [ -e $TARGETUF2 ]
then
    rm $TARGETUF2
fi
mkdir -p "$BUILD_DIR"

cp build/yarg-lang.uf2 $TARGETUF2
for test in cheese scone interrupt alarm blinky main coroutine-flash multicore-flash timed-flash
do
    $HOSTYARG compile --interpreter bin/cyarg --source "test/hardware/$test.ya" --output "$BUILD_DIR/$test.yb"
    $HOSTYARG cp -fs $TARGETUF2 -src "$BUILD_DIR/$test.yb" -dest "$test.yb"
done

for specimen in hello_led hello_button
do
    $HOSTYARG compile --interpreter bin/cyarg --source "yarg/specimen/$specimen.ya" --output "$BUILD_DIR/$specimen.yb"
    $HOSTYARG cp -fs $TARGETUF2 -src "$BUILD_DIR/$specimen.yb" -dest "$specimen.yb"
done

