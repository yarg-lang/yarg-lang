# yarg-lang

Yarg-Lang is a project to experiment with a dynamic language targetting microcontrollers. It has not yet made a release suitable for wide use.

Yarg aims to be a dedicated language for Microcontroller firmware development. It offers:

  - An interactive, on-device, REPL
  - Direct hardware access
  - Interupt based and multi-core multiprocessing
  - Many other modern language conveniences.

Microcontrollers (such as the $4 Raspberry Pi Pico, or the ESP32 family) are powerful computers, yet we commonly develop software for them with languages like C that were designed when resources were far more scarce. Yarg aims to provide a richer language, spending some of these resources to achive that. Of course, if you want to use modern language features, many general purpose languages are available in 'Micro', 'Tiny' or other cut-down versions of their implementation for microcontroller use. These implementations are faced with choices when the resources available do not support the same implementation possible in their original general purpose implementation. Do they try to be compatible (but over-expensive), or do they document a limitation compared to the full language implementation? Yarg aims to remove the limitations these choices impose, and offer modern langauge features, by being dedicated to the task of Microcontroller development.

Many samples for starting projects include polling, (while 'true'; sleep(x); do-stuff;), which can be wasteful of energy. How long is x? Modern microprocessors are designed to be normally off, and to wake when something interesting is happening. Yarg is a language designed with this in mind from the start.

## Aims

  - A dynamic environment for on-device prototyping
  - Tooling to deploy working prototypes
  - Sufficient static typing to reasonably add device specific code without writing C
  - Interop with C libraries available on device

Not (yet) intended for use. Additional documentation on the [wiki][wiki]

[wiki]: https://github.com/jhmcaleely/yarg-lang/wiki

| dir | Description |
| :--- | :--- |
| `cyarg/` | yarg implementation in C |
| `hostyarg/` | host tooling for yarg maintenance |
| `tools/` | Miscellaneous tools |
| `vscode-yarg/` | A VS Code Language Extension for Yarg |
| `yarg/specimen/` | Samples of Yarg |
| `yarg/specimen/conway-life-display` | A Yarg implemention of: [jhmcaleely/conway-life-display](https://github.com/jhmcaleely/conway-life-display) |
| `yarg/specimen/todo` | Things that don't work yet |
| `yarg/test/` | A Test Suite |

## Samples

### Blinky (aka 'Hello World' with a LED)

`yarg/specimen/blinky.ya`:
```
import("gpio");

gpio_init(pico_led);
gpio_set_direction(pico_led, GPIO_OUT);

var n = 5;
bool state = false;

while (n > 0) {
    state = !state;
    gpio_put(pico_led, state);
    sleep_ms(0d500);
    n = n - 1;
}
```
There's some work to be done here - constants should not need the '0d' decorator, and sleep_ms is currently a thin shim over a C function. I'd like to replace that with a native Yarg implementation.

### Button Press

`yarg/specimen/button.ya`:
``` 
import("gpio");

var state1 = false;
const gpio_button1 = 0d2;
const gpio_led1 = 0d3;

fun gpio_callback(num, events) {
    state1 = !state1;
    gpio_put(gpio_led1, state1);
}

gpio_init(gpio_led1);
gpio_set_direction(gpio_led1, GPIO_OUT);

gpio_init(gpio_button1);
gpio_set_irq_enabled_with_callback(gpio_button1, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, gpio_callback);
```
The interesting stuff hides in the GPIO library, allowing any suitable yarg function to be called back when the interrupt is raised by the button.



## Name

[Cornish Yarg](https://en.wikipedia.org/wiki/Cornish_Yarg) is a cheese I enjoy.
