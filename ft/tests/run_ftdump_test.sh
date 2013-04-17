#!/usr/bin/env bash

if [[ $# -ne 3 ]]; then exit 1; fi

test=$1; shift
outf=$1; shift
dump=$1; shift

set -e
$test
$dump $outf
