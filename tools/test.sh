#!/bin/bash

TEST_ERROR=0

pushd hostyarg >/dev/null
go test ./... || TEST_ERROR=1
popd >/dev/null

./bin/yarg runtests -tests "test/hostyarg" -interpreter "bin/cyarg" || TEST_ERROR=1

./bin/cyarg test/bc/int.ya | bc -lLS 0 | diff - test/bc/int.expected || TEST_ERROR=1

exit $TEST_ERROR