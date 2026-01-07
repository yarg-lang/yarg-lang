# Xcode README - how to build cyarg with Xcode

cyarg can (in theory) be built by any reasonable C toolchain, and Xcode is one such.

If you want to use Xcode, the prerequistes are:

  - A Mac, with Xcode installed
  - Xcode selected as a default toolchain on the command line:

```
$ sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
```

  - A clone of the repo, with submodules

```
  $ git clone --recurse-submodules git@github.com:yarg-lang/yarg-lang.git
```

 - The Pico SDK and its dependencies (notably CMake) installed, per Raspberry Pi's instructions.

 - Use CMake to build the xcode project file

```
$ cd cyarg
$ cmake --preset xcode-host
```

The project file will appear as `cyarg/build/xcode-host/cyarg.xcodeproj`

Note that a command line build should work outside of xcode:

```
$ cd cyarg
$ cmake --build build/xcode-host
```

Note that if you have hostyarg successfully built (you should!), then the test suite can be run:

```
$ ./bin/hostyarg runtests -tests "yarg/test" -interpreter "cyarg/build/xcode-host/Debug/cyarg"
```
