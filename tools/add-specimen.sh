#!/bin/bash

FS_PATH="${1:-build/specimen-fs.uf2}"

pushd yarg/specimen > /dev/null

../../bin/yarg addfile -fs "../../$FS_PATH" -add alarm.ya
../../bin/yarg addfile -fs "../../$FS_PATH" -add blinky.ya
../../bin/yarg addfile -fs "../../$FS_PATH" -add hello_led.ya
../../bin/yarg addfile -fs "../../$FS_PATH" -add hello_button.ya
../../bin/yarg addfile -fs "../../$FS_PATH" -add cheese.ya
../../bin/yarg addfile -fs "../../$FS_PATH" -add coroutine-flash.ya
../../bin/yarg addfile -fs "../../$FS_PATH" -add multicore-flash.ya
../../bin/yarg addfile -fs "../../$FS_PATH" -add scone.ya
../../bin/yarg addfile -fs "../../$FS_PATH" -add timed-flash.ya
../../bin/yarg addfile -fs "../../$FS_PATH" -add button.ya
../../bin/yarg addfile -fs "../../$FS_PATH" -add serial-echo.ya
../../bin/yarg addfile -fs "../../$FS_PATH" -add serial-input.ya
../../bin/yarg addfile -fs "../../$FS_PATH" -add button_flash.ya
../../bin/yarg addfile -fs "../../$FS_PATH" -add dma-test.ya
../../bin/yarg addfile -fs "../../$FS_PATH" -add main.ya

popd > /dev/null
