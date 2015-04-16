#!/bin/sh
cd ..
export DPKG_BUILD_ROOT=debian
mkdir -p $DPKG_BUILD_ROOT
make install PREFIX=$DPKG_BUILD_ROOT/usr
dpkg -b $DPKG_BUILD_ROOT libnavajo-1.0.amd64.deb

