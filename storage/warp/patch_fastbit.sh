#!/bin/bash
pushd .
cd $1
cd src/fastbit-2.0.3/src
patch < ../../../fastbit203_fixes.patch
popd
