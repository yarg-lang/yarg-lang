#!/bin/bash

HOSTYARG="../../../bin/yarg"
TARGETUF2="../../../build/conway.uf2"


pushd yarg/specimen/conway-life-display > /dev/null
if [ -e $TARGETUF2 ]
then
    rm $TARGETUF2
fi

cp ../../../build/yarg-lang.uf2 $TARGETUF2
$HOSTYARG cp -fs $TARGETUF2 -src conway.ya -dest conway.ya
$HOSTYARG cp -fs $TARGETUF2 -src cube_bit.ya -dest cube_bit.ya
$HOSTYARG cp -fs $TARGETUF2 -src main.ya -dest main.ya
popd > /dev/null
