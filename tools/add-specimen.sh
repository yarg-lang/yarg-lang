#!/bin/bash

FS_PATH="${1:-build/specimen-fs.uf2}"

pushd yarg/specimen > /dev/null

../../bin/yarg cp -fs "../../$FS_PATH" -src alarm.ya -dest alarm.ya
../../bin/yarg cp -fs "../../$FS_PATH" -src blinky.ya -dest blinky.ya
../../bin/yarg cp -fs "../../$FS_PATH" -src hello_led.ya -dest hello_led.ya
../../bin/yarg cp -fs "../../$FS_PATH" -src hello_button.ya -dest hello_button.ya
../../bin/yarg cp -fs "../../$FS_PATH" -src cheese.ya -dest cheese.ya
../../bin/yarg cp -fs "../../$FS_PATH" -src coroutine-flash.ya -dest coroutine-flash.ya
../../bin/yarg cp -fs "../../$FS_PATH" -src multicore-flash.ya -dest multicore-flash.ya
../../bin/yarg cp -fs "../../$FS_PATH" -src scone.ya -dest scone.ya
../../bin/yarg cp -fs "../../$FS_PATH" -src timed-flash.ya -dest timed-flash.ya
../../bin/yarg cp -fs "../../$FS_PATH" -src button.ya -dest button.ya
../../bin/yarg cp -fs "../../$FS_PATH" -src serial-echo.ya -dest serial-echo.ya
../../bin/yarg cp -fs "../../$FS_PATH" -src serial-input.ya -dest serial-input.ya
../../bin/yarg cp -fs "../../$FS_PATH" -src button_flash.ya -dest button_flash.ya
../../bin/yarg cp -fs "../../$FS_PATH" -src dma-test.ya -dest dma-test.ya
../../bin/yarg cp -fs "../../$FS_PATH" -src main.ya -dest main.ya

popd > /dev/null
