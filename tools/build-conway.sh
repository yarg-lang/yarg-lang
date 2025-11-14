#!/bin/bash

HOSTYARG="../../../bin/hostyarg"
TARGETUF2="../../../build/conway.uf2"


pushd yarg/specimen/conway-life-display > /dev/null
if [ -e $TARGETUF2 ]
then
    rm $TARGETUF2
fi

cp ../../../build/yarg-lang.uf2 $TARGETUF2
$HOSTYARG addfile -fs $TARGETUF2 -add conway.ya
$HOSTYARG addfile -fs $TARGETUF2 -add cube_bit.ya
$HOSTYARG addfile -fs $TARGETUF2 -add main.ya
popd > /dev/null
