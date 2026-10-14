# cyarg

A bytecode interpreter for Yarg, implemented in C.

Targets microcontroller devices, currently starting with the Raspberry Pi Pico (RP2040)

## Host Build

The host build contains a test system for ISRs as host ('pthreads') threads. TODO: add multicore support similarly.

## Pico Build

The cmake preset 'pico' will build cyarg/cyarg for the Raspberry Pi Pico (RP2040). It assumes that the Raspberry Pi VS Code extension is in use, and downloaded the Pico tools, SDK and toolchains into ~/.pico-sdk . It assumes two settings have been adjusted from the default, as recommended in the Extension docs:

https://github.com/raspberrypi/pico-vscode

"For more complex projects, such as those with multiple executables or when the project name is defined as a variable, this extension can integrate with the CMake Tools extension to enhance CMake parsing. You can enable CMake Tools integration during project generation under the Advanced Options. Additionally, to manually enable it, adjust the following settings in your settings.json:

  - raspberry-pi-pico.cmakeAutoConfigure: Set from true to false.
  - raspberry-pi-pico.useCmakeTools: Set from false to true."
