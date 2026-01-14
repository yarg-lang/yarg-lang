#!/bin/bash

if [ -e build/specimen-fs.uf2 ]
then
    rm build/specimen-fs.uf2
fi

./bin/hostyarg format -fs build/specimen-fs.uf2
./tools/add-yarg-stdlib.sh build/specimen-fs.uf2
./tools/add-specimen.sh build/specimen-fs.uf2