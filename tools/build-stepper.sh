#!/bin/bash

HOSTYARG="../../../bin/hostyarg"
TARGETUF2="../../../build/stepper.uf2"


pushd yarg/specimen/stepper-traverse > /dev/null
if [ -e $TARGETUF2 ]
then
    rm $TARGETUF2
fi

cp ../../../build/yarg-lang.uf2 $TARGETUF2
$HOSTYARG addfile -fs $TARGETUF2 -add stepper.ya
popd > /dev/null
