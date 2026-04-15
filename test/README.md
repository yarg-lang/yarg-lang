# Test Suites

test/ contains test suites:

`yarg-expect/` - the primary Yarg Language test suite, hosted by `yarg`.

`bc/` - a 'bc' based test suite of the cyarg/big-int integer library.

`cyarg/` - a test suite for the cyarg interfaces to yarg and hosted yarg scripts.

`hardware/` - a minimal smoketest to ensure Yarg is functional on pico hardware.

`benchmark/` - ad-hoc benchmarks.

## yarg-expect (Yarg Language)

A test suite that exercises the Yarg Language. Each file is parsed twice, once by `yarg` for 'expect'ed results, and then by `cyarg` itself. 

See [yarg-expect_test.md](../docs/yarg-expect_test.md).

Sample Usage:

```
./bin/yarg runtests -tests "test/yarg-expect" -interpreter "bin/cyarg"
```

As a convenience, the suite can be run from the repo's root directory with:

```
% ./test/yarg-expect-run.sh
```


## bc

A test suite that generates an input script for bc. Targets the big-int implementation in cyarg. Each test evaluates to an expected value, and any output becomes an error.

Sample usage:

```
% ./test/bc-run.sh
./test/bc-run.sh REPETITIONS=200, SEED=15415
```

The numbers are logged in build/bc-seed.txt.

The seed and a number of repetitions can be passed in to the script to reproduce problems.

Optionally, a number of iterations can be specified on its own.

eg:

```
% ./test/bc-run.sh 500
```
or
```
% ./test/bc-run.sh 200 15415
```

The first case is intended for use by a CI build to run more iterations than a local build, and the second case is intended to be used to reproduce failures.


## cyarg

A simple test to exercise the command line interface of `cyarg`, as required by `yarg`

```
% ./test/cyarg-run.sh
```

## hardware

A minimal smoke test to be run manually on a pico. Except hello_button, all will run on a stock pico with no additional peripherals. See [hello_button.pdf](../yarg/specimen/circuit-diagrams/hello_button.pdf) for the circuit needed for [hello_button.ya](../yarg/specimen/hello_button.ya)

Runs a suite that establishes yarg is mimimally functional, can import scripts, and can run the two examples from the [README.md](../README.md). Requires manual interaction to reset the board and confirm correct peripheral behaviour.

```
./tools/build-test-hardware.sh
```

the file `build/test-hardware.uf2` can be flashed on any pico. See the [hardware/README.md](hardware/README.md) for further instructions.

## benchmark

Ad-hoc benchmarks that output a report when run, eg:

```
./bin/cyarg test/benchmark/fib.ya
```

The results are only really meaningful when run on a device, and not on the host.