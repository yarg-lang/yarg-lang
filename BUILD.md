# Building Yarg

Note that using yarg on a device does not require building it from source. Installing the uf2 on the device, and installing hostyarg is sufficient. Even new board variations will often use an existing binary build of Yarg. Grab a build from [releases][rel], and follow the README instructions within. 

With that said, some ports will require work with the yarg runtime, and contributing to Yarg itself is also appreciated!

## Machine Setup

The host machine requires:

  * A C development toolchain, eg Xcode commandline tools.
  * The [Raspberry Pi Pico SDK][picosdk] (currently v2.2.0), and it's dependencies (notably picotool).

Currently this repo is only tested on Mac OS and the [Raspberry Pi Pico][pico1] (not Pico W or Pico 2).

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
Total tests: 1002, passed: 1002
```

## Cleanup

To clear all build atrefacts from the source tree:

`$ ./clean.sh`

[rel]: https://github.com/jhmcaleely/yarg-lang/releases
[picosdk]: https://www.raspberrypi.com/documentation/microcontrollers/c_sdk.html
[pico1]: https://datasheets.raspberrypi.com/pico/pico-product-brief.pdf
