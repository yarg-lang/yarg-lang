#!/bin/bash

HOSTYARG="../../bin/yarg"
TARGETUF2="../../build/benchmark.uf2"


pushd test/benchmark > /dev/null
if [ -e $TARGETUF2 ]
then
    rm $TARGETUF2
fi

cp ../../build/yarg-lang.uf2 $TARGETUF2

for y in binary_trees.ya equality.ya fib.ya instantiation.ya \
         int-perform.ya invocation.ya method_call.ya \
         properties.ya string_equality.ya trees.ya zoo_batch.ya \
         zoo.ya stable-interrupt.ya
do
    $HOSTYARG cp -fs $TARGETUF2 -src $y -dest $y
done
popd > /dev/null
