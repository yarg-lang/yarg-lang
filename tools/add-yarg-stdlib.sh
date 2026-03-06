#!/bin/bash

FS_PATH="${1:-build/specimen-fs.uf2}"

pushd yarg/specimen > /dev/null

../../bin/yarg cp -fs "../../$FS_PATH" -src gpio.ya -dest gpio.ya
../../bin/yarg cp -fs "../../$FS_PATH" -src irq.ya -dest irq.ya
../../bin/yarg cp -fs "../../$FS_PATH" -src machine.ya -dest machine.ya
../../bin/yarg cp -fs "../../$FS_PATH" -src timer.ya -dest timer.ya
../../bin/yarg cp -fs "../../$FS_PATH" -src uart.ya -dest uart.ya
../../bin/yarg cp -fs "../../$FS_PATH" -src reset.ya -dest reset.ya
../../bin/yarg cp -fs "../../$FS_PATH" -src clock.ya -dest clock.ya
../../bin/yarg cp -fs "../../$FS_PATH" -src pio.ya -dest pio.ya
../../bin/yarg cp -fs "../../$FS_PATH" -src pio-instructions.ya -dest pio-instructions.ya
../../bin/yarg cp -fs "../../$FS_PATH" -src dma.ya -dest dma.ya
../../bin/yarg cp -fs "../../$FS_PATH" -src ws2812.ya -dest ws2812.ya
../../bin/yarg cp -fs "../../$FS_PATH" -src apa102.ya -dest apa102.ya

popd > /dev/null
