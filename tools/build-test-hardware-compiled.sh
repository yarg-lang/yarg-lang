#!/bin/bash

HOSTYARG="../../bin/yarg"
TARGETUF2="../../build/test-hardware-compiled.uf2"


BUILD_DIR=../../build/test-hardware-compiled

pushd test/hardware > /dev/null
if [ -e $TARGETUF2 ]
then
    rm $TARGETUF2
fi

mkdir -p "$BUILD_DIR"
cp ../../build/yarg-lang.uf2 $TARGETUF2
for test in cheese scone interrupt alarm blinky main coroutine-flash multicore-flash timed-flash
do
    $HOSTYARG compile --interpreter ../../bin/cyarg --source "$test.ya" --output "$BUILD_DIR/$test.yb"
    $HOSTYARG cp -fs $TARGETUF2 -src "$BUILD_DIR/$test.yb" -dest "$test.yb"
done

popd > /dev/null
