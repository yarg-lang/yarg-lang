#!/bin/bash
REPETITIONS=${1:-200}
SEED=${2:-$RANDOM}

if ! [[ "$REPETITIONS" =~ ^[1-9][0-9]*$ ]]; then
  echo "usage: $0 [repetitions]" >&2
  exit 2
fi

if ! [[ "$SEED" =~ ^[0-9]+$ ]]; then
  echo "usage: $0 [repetitions] [seed]" >&2
  exit 2
fi

if [ ! -d "build" ]
then
    mkdir build
fi

echo "${0} REPETITIONS=$REPETITIONS, SEED=$SEED"
echo "REPETITIONS=$REPETITIONS, SEED=$SEED" >> build/bc-seeds.txt

RESULT=0
./bin/cyarg --lib yarg/specimen test/bc/int.ya -- "$SEED" "$REPETITIONS" | bc -lLS 0 | diff - test/bc/int.expected || RESULT=$?

exit $RESULT