#!/bin/bash

# a specimen script to get a vanilla Ubuntu 25.10 VM ready for building
# You may prefer an alternative approach.
# Note that the C++ in cyarg/test-system requires GCC 14 or later. 15 appears
# to be the default in Ubuntu 25.10.

sudo apt -y install build-essential
sudo apt -y install golang cmake ninja-build
