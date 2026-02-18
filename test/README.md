# Test Suites

test/ contains test suites:

`bc/` - a 'bc' based test suite of the cyarg/big-int integer library.

`benchmark/` - ad-hoc benchmarks

## bc

A test suite that generates an input script for bc. Each test evaluates to an expected value, and any output becomes an error.

Sample usage:

```
./bin/cyarg test/bc/int.ya | bc -lLS 0 | diff - test/bc/int.expected
```

Note pass through diff to get an exitcode that is non-zero on error.

## benchmark

Ad-hoc benchmarks that output a report when run, eg:

```
./bin/cyarg test/benchmark/fib.ya
```

The results are only really meaningful when run on a device, and not on the host.