#!/usr/bin/env bash

set -e

test $# -ge 4

bin=$1; shift
size=$1; shift
runs=$1; shift
abortcode=$1; shift

mkdir -p $TOKU_TEST_FILENAME
$bin -C -n $size -l
$bin -C -i 0 -n $size -l
for (( i = 1; i < $runs; i++ ))
do
    echo -n "$i: " && date
    set +e
    $bin -c -i $i -n $size -l -X novalgrind 2>$TOKU_TEST_FILENAME/error.$i
    test $? -eq $abortcode || exit 1
    set -e
    grep -q 'HAPPY CRASH' $TOKU_TEST_FILENAME/error.$i
done
