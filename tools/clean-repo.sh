#!/bin/bash

if [ -d bin ]
then
    rm -R bin
fi
if [ -d build ]
then
    rm -R build
fi

if [ -e hostyarg/hostyarg ]
then
    rm hostyarg/hostyarg
fi

if [ -e hostyarg/test.uf2 ]
then
    rm hostyarg/test.uf2
fi

if [ -d cyarg/build ]
then
    rm -R cyarg/build
fi
