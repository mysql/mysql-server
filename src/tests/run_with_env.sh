#!/usr/bin/env bash

if [[ $# -lt 3 ]]; then exit 1; fi

bin=$1; shift
saveddir=$1; shift
envdir=$1; shift

rm -rf $envdir
cp -r $saveddir $envdir
$bin "$@"
