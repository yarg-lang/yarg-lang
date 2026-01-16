# Building Yarg

Note: Using Yarg on a device, and wrting new Yarg for a device does not require building Yarg from source. Yarg usage only requires a binary of hostyarg (currently built by go on your host PC), and an image file for your target board (currently a .uf2 for the Pico). Grab a build from [releases][rel], and follow the README instructions within. 

With that said, contributing to Yarg itself is appreciated!

## Machine Setup

The host machine requires:

  * A C development toolchain, eg Xcode commandline tools, or a GCC based toolchain (gcc must be v14 or higher)
  * go (golang), for the test harness and target image generation tools (`hostyarg`)
  * The [Raspberry Pi Pico SDK][picosdk] (currently v2.2.0), and it's dependencies (notably picotool), if you want to build for the Pico.
  * ./tools/setup-ubuntu.sh is a specimen script to configure an Ubuntu 25.10 VM

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
$ ./tools/build-host.sh
$ ./tools/build-target-pico.sh
```

or, if you've already cloned the repo:

```
$ cd yarg-lang
$ git submodule update --init --recursive
$ ./tools/build-host.sh
$ ./tools/build-target-pico.sh
```

This produces uf2 files and host binaries in bin/ and build/

if you don't want a pico target (then skip ./tools/build-target-pico.sh)

## Testing

The script:

`$ ./tools/test.sh`

Will run the Yarg test suite. A sample (good) output looks like:

```
% ./tools/test.sh 
Interpreter: bin/cyarg
Tests: yarg/test
Total tests: 1044, passed: 1044
```

## Cleanup

To clear all build atrefacts from the source tree:

`$ ./tools/clean-repo.sh`

## CI Builds

When you push to the upstream yarg-lang repo, the CI will run a suite of tests on your code, which all have
shell scripts in their name. These scripts will do the local equivalent, if you call them from a shell on host.

All of the offered tests must pass for the PR to be tested before merge.

Releases are made from pushes to main, which is a repeat of some of the CI tests, but only those parts needed for a release tarball to be created. Again, `tools/build-release.sh` is a local equaivalent.

[rel]: https://github.com/jhmcaleely/yarg-lang/releases
[picosdk]: https://www.raspberrypi.com/documentation/microcontrollers/c_sdk.html
[pico1]: https://datasheets.raspberrypi.com/pico/pico-product-brief.pdf
