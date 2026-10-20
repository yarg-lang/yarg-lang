#!/bin/sh

HOSTYARG=./bin/yarg

for file in \
./hostyarg/testdata/stale_cheese \
./hostyarg/testdata/fresh_cheese \
./yarg/lang/yarg \
./yarg/stdlib/xip_library \
./yarg/stdlib/repl
do
    echo "Processing $file"
    $HOSTYARG compile --interpreter bin/cyarg --source "$file.ya" --output "$file.yb"
done
