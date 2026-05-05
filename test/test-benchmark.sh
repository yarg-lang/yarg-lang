#!/bin/bash

BENCHMARKS="fib equality string_equality instantiation invocation \
                method_call properties trees zoo zoo_batch binary_trees int-perform"

BENCH_ERROR=0

for bench in $BENCHMARKS
do
    ./bin/yarg run --interpreter bin/cyarg --lib yarg/specimen test/benchmark/$bench.ya || BENCH_ERROR=1
done

exit $BENCH_ERROR