#!/bin/sh
OSPRAY_DIR=$HOME/software/ospray-2.7.0.x86_64.linux
GLM_DIR=$HOME/software/glm-0.9.9.9

g++ \
    -O3 -W -Wall \
    -shared -fPIC \
    -std=c++11 \
    -I $OSPRAY_DIR/include \
    -I $OSPRAY_DIR/include/ospray/ospray_testing \
    -L $OSPRAY_DIR/lib \
    `python -m pybind11 --includes` \
    -I $GLM_DIR/include \
    ospray.cpp \
    -o ospray`python3-config --extension-suffix` \
    -lospray
    #-lospray_testing
