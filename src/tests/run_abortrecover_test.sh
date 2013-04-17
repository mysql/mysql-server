#!/usr/bin/env bash

if [[ $# -ne 1 ]]; then exit 1; fi

bin=$1; shift

$bin --test
if [[ $? -eq 0 ]]
then
    echo $bin --test did not crash
    exit 1
else
    set -e
    $bin --recover
fi
