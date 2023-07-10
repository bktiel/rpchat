#!/bin/sh

[ ! -d "build" ] && mkdir build

cmake -Bbuild
ln -s $PWD/codechecker.sh $PWD/build/codechecker.sh
cd build && make