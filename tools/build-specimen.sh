#!/bin/bash

pushd yarg/specimen > /dev/null
if [ -e ../../build/specimen-fs.uf2 ]
then
    rm ../../build/specimen-fs.uf2
fi

../../bin/hostyarg format -fs ../../build/specimen-fs.uf2
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add gpio.ya
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add irq.ya
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add machine.ya
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add timer.ya
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add uart.ya
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add reset.ya
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add clock.ya
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add pio.ya
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add pio-instructions.ya
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add dma.ya
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add ws2812.ya
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add apa102.ya

../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add alarm.ya
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add blinky.ya
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add hello_led.ya
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add hello_button.ya
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add cheese.ya
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add coroutine-flash.ya
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add multicore-flash.ya
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add scone.ya
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add timed-flash.ya
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add button.ya
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add serial-echo.ya
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add serial-input.ya
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add button_flash.ya
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add dma-test.ya
../../bin/hostyarg addfile -fs ../../build/specimen-fs.uf2 -add main.ya

popd > /dev/null

