#!/bin/bash

set -e

test $# -ge 1

bin=$1; shift
valgrind="$@"

$valgrind $bin -n 1000
$bin