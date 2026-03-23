#!/bin/bash

./test/cyarg/cyarg.sh 2>&1 | diff - test/cyarg/cyarg.expected