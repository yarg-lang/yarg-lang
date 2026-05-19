#!/bin/bash

SOURCE="${1:-yarg/specimen/syntax.ya}"

rm build/debug.uf2
cp build/yarg-lang.uf2 build/debug.uf2
./bin/yarg compile --interpreter bin/cyarg --source "$SOURCE" --output build/syntax.yb
./bin/yarg cp -fs build/debug.uf2 -src "build/syntax.yb" -dest syntax.yb
picotool load -f build/debug.uf2
