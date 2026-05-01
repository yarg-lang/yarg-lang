#!/bin/bash

BENCHMARKS="fib int-perform equality string_equality instantiation invocation \
                method_call properties trees zoo zoo_batch binary_trees"

./tools/build-benchmark.sh
picotool load -f build/benchmark.uf2
sleep 2
for bench in $BENCHMARKS
do
    ./bin/yarg run device:$bench.ya
done
