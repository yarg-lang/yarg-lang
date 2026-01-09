# Building Yarg

Note: Using Yarg on a device, and wrting new Yarg for a device does not require building Yarg from source. Yarg usage only requires a binary of hostyarg (currently built by go on your host PC), and an image file for your target board (currently a .uf2 for the Pico). Grab a build from [releases][rel], and follow the README instructions within. 

With that said, contributing to Yarg itself is appreciated!

## Machine Setup

The host machine requires:

  * A C development toolchain, eg Xcode commandline tools, or a GCC based toolchain
  * The [Raspberry Pi Pico SDK][picosdk] (currently v2.2.0), and it's dependencies (notably picotool), if you want to build for the Pico.

## Pico SDK dependency

The build needs to locate the Pico SDK and associated tools when it builds targets for the Pico. You can supply an environment with PICO_SDK_PATH set and picotool fully installed. Alternatively, you can use CMakeUserPresets.json to supply the necessary information. On my machine, I use cyarg\CMakeUserPresets.json with these entries:

```
{
    "version": 8,
    "configurePresets": [
        {
            "name": "pico",
            "displayName": "Local Pico (SDK 2.2.0, 14_2_Rel1 for Pico/RP2040)",
            "inherits": "target-pico",
            "cacheVariables": {
                "PICO_SDK_PATH": "$env{HOME}/.pico-sdk/sdk/2.2.0",
                "PICO_TOOLCHAIN_PATH": "$env{HOME}/.pico-sdk/toolchain/14_2_Rel1"
            } 
        },
        {
            "name": "pico-debug",
            "displayName": "Pico Debug (SDK 2.2.0, 14_2_Rel1 for Pico/RP2040)",
            "inherits": "target-pico-debug",
            "cacheVariables": {
                "PICO_SDK_PATH": "$env{HOME}/.pico-sdk/sdk/2.2.0",
                "PICO_TOOLCHAIN_PATH": "$env{HOME}/.pico-sdk/toolchain/14_2_Rel1"
            } 
        }
    ]
}
```

In my case the Raspberry Pi Pico extension for VSCode manages the dependencies in ~/.pico-sdk for me.

## First time build:

```
$ git clone --recurse-submodules git@github.com:jhmcaleely/yarg-lang.git
$ ./build.sh
```

or, if you've already cloned the repo:

```
$ cd yarg-lang
$ git submodule update --init --recursive
$ ./build.sh
```

This produces uf2 files and host binaries in bin/ and build/

## Testing

The script:

`$ ./test.sh`

Will run the Yarg test suite. A sample (good) output looks like:

```
% ./test.sh 
Interpreter: bin/cyarg
Tests: yarg/test
Total tests: 1044, passed: 1044
```

## Cleanup

To clear all build atrefacts from the source tree:

`$ ./clean.sh`

[rel]: https://github.com/jhmcaleely/yarg-lang/releases
[picosdk]: https://www.raspberrypi.com/documentation/microcontrollers/c_sdk.html
[pico1]: https://datasheets.raspberrypi.com/pico/pico-product-brief.pdf
