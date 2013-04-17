#!/usr/bin/env bash

if [[ $# -lt 2 ]]; then exit 1; fi

bin=$1; shift
abortcode=$1; shift

$bin --test
if [[ $? -ne $abortcode ]]
then
    echo $bin --test did not return $abortcode
    exit 1
else
    set -e
    $bin --recover
fi
