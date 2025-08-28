#!/bin/bash

TARGETUF2=build/yarg-lang-pico-0.1.0.uf2

if [ -e release/yarg-lang-pico-0.1.0.tgz ]
then
    rm release/yarg-lang-pico-0.1.0.tgz
fi

if [ -e $TARGETUF2 ]
then
    rm $TARGETUF2
fi

if [ -e build/release ]
then
    rm -Rf build/release
fi

mkdir build/release
mkdir build/release/extras

cp build/yarg-lang.uf2 $TARGETUF2



pushd yarg/specimen
../../bin/hostyarg addfile -fs ../../$TARGETUF2 -add alarm.ya
../../bin/hostyarg addfile -fs ../../$TARGETUF2 -add blinky.ya
../../bin/hostyarg addfile -fs ../../$TARGETUF2 -add hello_led.ya
../../bin/hostyarg addfile -fs ../../$TARGETUF2 -add hello_button.ya
../../bin/hostyarg addfile -fs ../../$TARGETUF2 -add coffee.ya
../../bin/hostyarg addfile -fs ../../$TARGETUF2 -add coroutine-flash.ya
../../bin/hostyarg addfile -fs ../../$TARGETUF2 -add multicore-flash.ya
../../bin/hostyarg addfile -fs ../../$TARGETUF2 -add scone.ya
../../bin/hostyarg addfile -fs ../../$TARGETUF2 -add timed-flash.ya
../../bin/hostyarg addfile -fs ../../$TARGETUF2 -add ws2812.ya
../../bin/hostyarg addfile -fs ../../$TARGETUF2 -add button.ya
popd

if [ ! -e release ]
then
    mkdir release
fi

cp $TARGETUF2 build/release
cp docs/release_README build/release/README
cp build/yarg-lang-0.1.0.vsix build/release/extras/

pushd build/release
tar -cvzf ../../release/yarg-lang-pico-0.1.0.tgz .
popd