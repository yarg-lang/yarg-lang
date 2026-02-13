# Building Yarg

Note: Using Yarg on a device, and wrting new Yarg for a device does not require building Yarg from source. Yarg usage only requires a binary of hostyarg (currently built by go on your host PC), and an image file for your target board (currently a .uf2 for the Pico). Grab a build from [releases][rel], and follow the README instructions within. 

With that said, contributing to Yarg itself is appreciated!

## Machine Setup

The host machine requires:

  * A C development toolchain, eg a GCC or CLang based toolchain (gcc must be v14 or higher)
  * `go` ([golang][golang]), for the test harness and target image generation tools (`hostyarg`)
  * The [Raspberry Pi Pico SDK][picosdk] (currently v2.2.0), and it's dependencies (notably `picotool`), if you want to build for the Pico.
  * `./tools/setup-ubuntu-vm.sh` is a specimen script to configure an Ubuntu 25.10 VM. It will installed the pico toolchain and sdk in pico-tooling/ if you supply `pico` as a parameter, eg:

  ```
  ./tools/setup-ubuntu-vm.sh pico
  ```

Optionally:

  * `vsce` should be installed to build the vscode-yarg project, to provide syntax highlighting in VSCode
  * `Xcode` and its default toolchain can be used to build host binaries on MacOS. `go` is required to test them. See [BUILD_XCode.md](docs/BUILD_XCode.md)

### Pico SDK dependency

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

First set up your environment to meet the requirements above, and then:

```
$ git clone --recurse-submodules git@github.com:<username>/yarg-lang.git
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
'Total tests' and 'passed' match, and are the same or higher than the last run on main.

## Cleanup

To clear all build atrefacts from the source tree:

`$ ./tools/clean-repo.sh`

## Configuration

This repo contains Yarg and various supporting tools and tests. These are built for Microcontroller devices (currently Raspberry Pi Pico), with supporting tools on a host PC (currently MacOS or Ubuntu, with Windows and other Distros in future). Additionally, Yarg itself can be built for use on host, for convenient development and testing.

You can build a target/device build (names used interchangably). A full build will also include support tooling (notably hostyarg) for the PC. If you want, you can build exclusively for host, producing a version of yarg that can be used from a command line. Tools such as xcode project files are available to support this.

Set YARG_DEVICE in your environment or similar (eg CMakePresets.json or CMakeUserPresets.json).

Use YARG_DEVICE="RASPBERRY_PI_PICO" for a Pico build, and YARG_DEVICE="GENERIC_HOST" for a build for use on a PC.

## CI Builds

[![Release (Pico)](https://github.com/yarg-lang/yarg-lang/actions/workflows/release.yml/badge.svg)](https://github.com/yarg-lang/yarg-lang/actions/workflows/release.yml)

When you push to the upstream yarg-lang repo, the CI will run a suite of tests on your code, which all have
shell scripts in their name. These scripts will do the local equivalent, if you call them from a shell on host.

All of the offered tests must pass for the PR to be tested before merge.

Releases are made from pushes to main, which is a repeat of some of the CI tests, but only those parts needed for a release tarball to be created. Again, `tools/build-release.sh` is a local equaivalent.

### Locally building as much of the repo as the CI does

use `tools/build-repo.sh` to build as many targets as your environment supports. It is assumed hosts will always build. if this script runs cleanly, so should both CI suites. Requires MacOS to target Xcode.


[rel]: https://github.com/jhmcaleely/yarg-lang/releases
[picosdk]: https://www.raspberrypi.com/documentation/microcontrollers/c_sdk.html
[pico1]: https://datasheets.raspberrypi.com/pico/pico-product-brief.pdf
[golang]: https://go.dev
