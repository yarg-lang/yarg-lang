#!/bin/bash

TEST_ERROR=0

./bin/hostyarg runtests -tests "yarg/test" -interpreter "bin/cyarg" || TEST_ERROR=1

exit $TEST_ERROR