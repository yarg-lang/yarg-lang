# Hardware test

A minimal hardware smoke test. Currently runs on a stock pico. Note that hello_button requires additional peripherals: [hello_button.pdf](../../yarg/specimen/circuit-diagrams/hello_button.pdf).

Run each in sequence.

```
cheese.ya
scone.ya
../../yarg/specimen/hello_led.ya
../../yarg/specimen/hello_button.ya
interrupt.ya
alarm.ya
blinky.ya
coroutine-flash.ya
multicore-flash.ya
timed-flash.ya
```

It is currently expected to 'reset' the board between some runs.