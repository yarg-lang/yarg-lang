#!/bin/bash

# build cyarg, assuming --compile does not require the .yb files embedded in it
# Use this cyarg to compile all the .yb files in the tree
# Then rebuild the c files that include the .yb files into cyarg
# finally, build cyarg again to pick up the new .yb files.

# if script leaves the tree dirty, those files need to be committed.

BUILD_ERROR=0

./tools/build-host.sh || BUILD_ERROR=1
./tools/build-committed-yarg.sh || BUILD_ERROR=1
./tools/build-cyarg-bootstrap.sh || BUILD_ERROR=1
./tools/build-host.sh || BUILD_ERROR=1

exit $BUILD_ERROR