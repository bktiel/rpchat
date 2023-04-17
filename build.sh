#!/bin/sh

[ ! -d "build" ] && mkdir build

cmake -Bbuild
cd build && make

