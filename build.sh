#!/bin/sh
O=$HOME/software/ospray-superbuild-git

g++ \
    -O3 -Wall \
    -shared -fPIC \
    -std=c++11 \
    -I $O/include \
    -L $O/lib \
    `python -m pybind11 --includes` \
    ospray.cpp \
    -o ospray`python3-config --extension-suffix` \
    -lospray
