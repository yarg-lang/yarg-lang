#!/bin/bash

FS_PATH="${1:-build/specimen-fs.uf2}"

pushd yarg/specimen > /dev/null

../../bin/hostyarg addfile -fs "../../$FS_PATH" -add alarm.ya
../../bin/hostyarg addfile -fs "../../$FS_PATH" -add blinky.ya
../../bin/hostyarg addfile -fs "../../$FS_PATH" -add hello_led.ya
../../bin/hostyarg addfile -fs "../../$FS_PATH" -add hello_button.ya
../../bin/hostyarg addfile -fs "../../$FS_PATH" -add cheese.ya
../../bin/hostyarg addfile -fs "../../$FS_PATH" -add coroutine-flash.ya
../../bin/hostyarg addfile -fs "../../$FS_PATH" -add multicore-flash.ya
../../bin/hostyarg addfile -fs "../../$FS_PATH" -add scone.ya
../../bin/hostyarg addfile -fs "../../$FS_PATH" -add timed-flash.ya
../../bin/hostyarg addfile -fs "../../$FS_PATH" -add button.ya
../../bin/hostyarg addfile -fs "../../$FS_PATH" -add serial-echo.ya
../../bin/hostyarg addfile -fs "../../$FS_PATH" -add serial-input.ya
../../bin/hostyarg addfile -fs "../../$FS_PATH" -add button_flash.ya
../../bin/hostyarg addfile -fs "../../$FS_PATH" -add dma-test.ya
../../bin/hostyarg addfile -fs "../../$FS_PATH" -add main.ya

popd > /dev/null
