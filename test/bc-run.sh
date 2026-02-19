#!/bin/bash

./bin/cyarg test/bc/int.ya | bc -lLS 0 | diff - test/bc/int.expected