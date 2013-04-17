#!/bin/bash

set -e

test $# -ge 2

bin=$1; shift
abortcode=$1; shift
valgrind="$@"

num_writes=$($valgrind $bin -q)
set +e
for (( i = 0; i < $num_writes; i++ ))
do
    $bin -C $i
    test $? -eq $abortcode || exit 1
done
