#!/bin/bash

for file in hostyarg/cmd/yarg/yarg hostyarg/yarg
do
    if [ -e $file ]
    then
        rm $file
    fi
done

for dir in bin build release cyarg/build
do
    if [ -d $dir ]
    then
        rm -R $dir
    fi
done
