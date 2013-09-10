#!/bin/bash

pushd $(dirname $0) &>/dev/null
SCRIPTDIR=$PWD
popd &>/dev/null

exec $SCRIPTDIR/run.fractal.tree.tests.bash --ctest_model=Experimental --commit=0 "$@"
