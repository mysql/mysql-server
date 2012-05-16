#!/bin/bash

set -e

test $# -ge 4

bin=$1; shift
size=$1; shift
runs=$1; shift
abortcode=$1; shift

mkdir -p dir.recover_stress.c.tdb
$bin -C -n $size -l
$bin -C -i 0 -n $size -l
for (( i = 1; i < $runs; i++ ))
do
    echo -n "$i: " && date
    set +e
    $bin -c -i $i -n $size -l -X novalgrind 2>dir.recover_stress.c.tdb/error.$i
    test $? -eq $abortcode || exit 1
    set -e
    grep -q 'HAPPY CRASH' dir.recover_stress.c.tdb/error.$i
done
