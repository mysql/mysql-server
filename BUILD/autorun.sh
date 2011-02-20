#!/bin/sh
# Create MySQL cmake configure wrapper

die() { echo "$@"; exit 1; }

# Use a configure script that will call CMake.
path=`dirname $0`
cp $path/cmake_configure.sh $path/../configure
chmod +x $path/../configure
