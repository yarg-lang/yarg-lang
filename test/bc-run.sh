#!/bin/bash

./bin/cyarg --lib yarg/specimen test/bc/int.ya | bc -lLS 0 | diff - test/bc/int.expected