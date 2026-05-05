#!/bin/bash

TEST_ERROR=0

pushd hostyarg >/dev/null
go test ./... || TEST_ERROR=1
popd >/dev/null

./test/cyarg-run.sh || TEST_ERROR=1

./bin/yarg runtests -tests "test/yarg-expect" -interpreter "bin/cyarg" -lib "yarg/specimen" || TEST_ERROR=1

./test/bc-run.sh || TEST_ERROR=1

./test/test-benchmark.sh || TEST_ERROR=1
echo "Benchmark run for execution correctness. Results are only meaningful on target"

exit $TEST_ERROR