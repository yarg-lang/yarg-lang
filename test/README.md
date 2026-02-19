# Test Suites

test/ contains test suites:

`hostyarg/` - the primary Yarg Language test suite, hosted by hostyarg.

`bc/` - a 'bc' based test suite of the cyarg/big-int integer library.

`benchmark/` - ad-hoc benchmarks

## hostyarg Yarg Language

A test suite that exercises the Yarg Language. Each file is parsed twice, once by hostyarg for expected results, and then by cyarg itself. 

See [yarg_test.md](../docs/yarg_test.md).

Sample Usage:

```
./bin/hostyarg runtests -tests "test/hostyarg" -interpreter "bin/cyarg"
```

As a convenience, the suite can be run from the repo's root directory with:

```
% ./test/hostyarg-run.sh
```


## bc

A test suite that generates an input script for bc. Each test evaluates to an expected value, and any output becomes an error.

Sample usage:

```
./bin/cyarg test/bc/int.ya | bc -lLS 0 | diff - test/bc/int.expected
```

Note pass through diff to get an exitcode that is non-zero on error.

As a convenience, the tests can be run from the repo's root diretory with:

```
% ./test/bc-run.sh
```

## benchmark

Ad-hoc benchmarks that output a report when run, eg:

```
./bin/cyarg test/benchmark/fib.ya
```

The results are only really meaningful when run on a device, and not on the host.