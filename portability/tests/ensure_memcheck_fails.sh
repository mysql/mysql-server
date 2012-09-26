#!/usr/bin/env bash

test $# -ge 1 || exit 1

bin=$1; shift
valgrind=$@

$valgrind --log-file=$bin.check.valgrind $bin >$bin.check.output 2>&1
if [[ $? = 0 ]]
then
    lines=$(cat $bin.check.valgrind | wc -l)
    if [[ lines -ne 0 ]]
    then
        cat $bin.check.valgrind
        exit 0
    else
        exit 1
    fi
else
    exit 0
fi
