#!/bin/bash

pushd yarg/specimen > /dev/null
if [ -e ../../build/specimen.uf2 ]
then
    rm ../../build/specimen.uf2
fi

../../bin/hostyarg format -fs ../../build/specimen.uf2
../../bin/hostyarg addfile -fs ../../build/specimen.uf2 -add gpio.ya
../../bin/hostyarg addfile -fs ../../build/specimen.uf2 -add irq.ya
../../bin/hostyarg addfile -fs ../../build/specimen.uf2 -add machine.ya
../../bin/hostyarg addfile -fs ../../build/specimen.uf2 -add alarm.ya
../../bin/hostyarg addfile -fs ../../build/specimen.uf2 -add blinky.ya
../../bin/hostyarg addfile -fs ../../build/specimen.uf2 -add coffee.ya
../../bin/hostyarg addfile -fs ../../build/specimen.uf2 -add coroutine-flash.ya
../../bin/hostyarg addfile -fs ../../build/specimen.uf2 -add multicore-flash.ya
../../bin/hostyarg addfile -fs ../../build/specimen.uf2 -add scone.ya
../../bin/hostyarg addfile -fs ../../build/specimen.uf2 -add timed-flash.ya
../../bin/hostyarg addfile -fs ../../build/specimen.uf2 -add ws2812.ya
../../bin/hostyarg addfile -fs ../../build/specimen.uf2 -add button.ya
../../bin/hostyarg addfile -fs ../../build/specimen.uf2 -add main.ya

popd > /dev/null

