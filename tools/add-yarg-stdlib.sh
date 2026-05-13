#!/bin/bash

FS_PATH="${1:-build/specimen-fs.uf2}"

bin/yarg cp -fs "$FS_PATH" -src yarg/specimen/gpio.ya -dest gpio.ya
bin/yarg cp -fs "$FS_PATH" -src yarg/specimen/irq.ya -dest irq.ya
bin/yarg cp -fs "$FS_PATH" -src yarg/specimen/machine.ya -dest machine.ya
bin/yarg cp -fs "$FS_PATH" -src yarg/specimen/timer.ya -dest timer.ya
bin/yarg cp -fs "$FS_PATH" -src yarg/specimen/uart.ya -dest uart.ya
bin/yarg cp -fs "$FS_PATH" -src yarg/specimen/reset.ya -dest reset.ya
bin/yarg cp -fs "$FS_PATH" -src yarg/specimen/clock.ya -dest clock.ya
bin/yarg cp -fs "$FS_PATH" -src yarg/specimen/pio.ya -dest pio.ya
bin/yarg cp -fs "$FS_PATH" -src yarg/specimen/pio-instructions.ya -dest pio-instructions.ya
bin/yarg cp -fs "$FS_PATH" -src yarg/specimen/dma.ya -dest dma.ya
bin/yarg cp -fs "$FS_PATH" -src yarg/specimen/ws2812.ya -dest ws2812.ya
bin/yarg cp -fs "$FS_PATH" -src yarg/specimen/apa102.ya -dest apa102.ya
bin/yarg cp -fs "$FS_PATH" -src yarg/specimen/m0plus.ya -dest m0plus.ya
bin/yarg cp -fs "$FS_PATH" -src yarg/specimen/repl.ya -dest repl.ya
bin/yarg cp -fs "$FS_PATH" -src yarg/specimen/yarg.ya -dest yarg.ya
bin/yarg cp -fs "$FS_PATH" -src yarg/specimen/cyarg.ya -dest cyarg.ya
