#!/bin/bash

TEST_ERROR=0

pushd hostyarg >/dev/null
go test -args -interpreter "../bin/cyarg"|| TEST_ERROR=1
popd >/dev/null

./bin/hostyarg runtests -tests "yarg/test" -interpreter "bin/cyarg" || TEST_ERROR=1

exit $TEST_ERROR