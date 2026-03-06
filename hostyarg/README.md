# hostyarg

Host based tooling for creating and manipulating yarg images for devices.

Currently has two core features

  - Running cyarg against the test suites in test/hostyarg
  - Building Pico UF2 images that contain cyarg and useful Yarg .ya sources. An in-memory image is built up with littlefs and then written to the UF2 file.

## Building

```
$ go build ./cmd/yarg
```

# Testing

```
$ go test [-args -intepreter ../path/to/cyarg]
```

## Installation

Install using the go package manager:

```
$ go install github.com/yarg-lang/yarg-lang/hostyarg/cmd/yarg@latest
```

## Usage

```
$ yarg [cp|format|ls|runtests]
```
