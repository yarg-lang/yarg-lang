#!/bin/bash

FS_PATH="${1:-build/specimen-fs.uf2}"

pushd yarg/specimen > /dev/null

../../bin/hostyarg addfile -fs "../../$FS_PATH" -add gpio.ya
../../bin/hostyarg addfile -fs "../../$FS_PATH" -add irq.ya
../../bin/hostyarg addfile -fs "../../$FS_PATH" -add machine.ya
../../bin/hostyarg addfile -fs "../../$FS_PATH" -add timer.ya
../../bin/hostyarg addfile -fs "../../$FS_PATH" -add uart.ya
../../bin/hostyarg addfile -fs "../../$FS_PATH" -add reset.ya
../../bin/hostyarg addfile -fs "../../$FS_PATH" -add clock.ya
../../bin/hostyarg addfile -fs "../../$FS_PATH" -add pio.ya
../../bin/hostyarg addfile -fs "../../$FS_PATH" -add pio-instructions.ya
../../bin/hostyarg addfile -fs "../../$FS_PATH" -add dma.ya
../../bin/hostyarg addfile -fs "../../$FS_PATH" -add ws2812.ya
../../bin/hostyarg addfile -fs "../../$FS_PATH" -add apa102.ya

popd > /dev/null
