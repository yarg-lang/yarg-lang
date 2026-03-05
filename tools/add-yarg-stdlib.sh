#!/bin/bash

FS_PATH="${1:-build/specimen-fs.uf2}"

pushd yarg/specimen > /dev/null

../../bin/yarg addfile -fs "../../$FS_PATH" -add gpio.ya
../../bin/yarg addfile -fs "../../$FS_PATH" -add irq.ya
../../bin/yarg addfile -fs "../../$FS_PATH" -add machine.ya
../../bin/yarg addfile -fs "../../$FS_PATH" -add timer.ya
../../bin/yarg addfile -fs "../../$FS_PATH" -add uart.ya
../../bin/yarg addfile -fs "../../$FS_PATH" -add reset.ya
../../bin/yarg addfile -fs "../../$FS_PATH" -add clock.ya
../../bin/yarg addfile -fs "../../$FS_PATH" -add pio.ya
../../bin/yarg addfile -fs "../../$FS_PATH" -add pio-instructions.ya
../../bin/yarg addfile -fs "../../$FS_PATH" -add dma.ya
../../bin/yarg addfile -fs "../../$FS_PATH" -add ws2812.ya
../../bin/yarg addfile -fs "../../$FS_PATH" -add apa102.ya

popd > /dev/null
