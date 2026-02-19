#!/bin/bash

TEST_ERROR=0

pushd hostyarg >/dev/null
go test -args -interpreter "../bin/cyarg"|| TEST_ERROR=1
popd >/dev/null

./bin/hostyarg runtests -tests "test/hostyarg" -interpreter "bin/cyarg" || TEST_ERROR=1

./bin/cyarg test/bc/int.ya | bc -lLS 0 | diff - test/bc/int.expected || TEST_ERROR=1

exit $TEST_ERROR