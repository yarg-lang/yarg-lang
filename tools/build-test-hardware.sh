#!/bin/bash

HOSTYARG="../../bin/yarg"
TARGETUF2="../../build/test-hardware.uf2"


pushd test/hardware > /dev/null
if [ -e $TARGETUF2 ]
then
    rm $TARGETUF2
fi

cp ../../build/yarg-lang.uf2 $TARGETUF2
$HOSTYARG cp -fs $TARGETUF2 -src cheese.ya -dest cheese.ya
$HOSTYARG cp -fs $TARGETUF2 -src scone.ya -dest scone.ya
$HOSTYARG cp -fs $TARGETUF2 -src alarm.ya -dest alarm.ya
$HOSTYARG cp -fs $TARGETUF2 -src blinky.ya -dest blinky.ya
$HOSTYARG cp -fs $TARGETUF2 -src main.ya -dest main.ya
$HOSTYARG cp -fs $TARGETUF2 -src coroutine-flash.ya -dest coroutine-flash.ya
$HOSTYARG cp -fs $TARGETUF2 -src multicore-flash.ya -dest multicore-flash.ya
$HOSTYARG cp -fs $TARGETUF2 -src timed-flash.ya -dest timed-flash.ya
$HOSTYARG cp -fs $TARGETUF2 -src ../../yarg/specimen/hello_led.ya -dest hello_led.ya
$HOSTYARG cp -fs $TARGETUF2 -src ../../yarg/specimen/hello_button.ya -dest hello_button.ya
popd > /dev/null
