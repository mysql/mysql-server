#!/usr/bin/env bash

set -e
test $# -ge 3

bin=$1; shift
errorfile=$1; shift
abortcode=$1; shift

set +e
$bin -X novalgrind -c $@ 2> $errorfile
test $? -eq $abortcode || { cat $errorfile; echo Error: no crash in $errorfile; exit 1; }
set -e
grep -q 'HAPPY CRASH' $errorfile || { cat $errorfile; echo Error: incorrect crash in $errorfile; exit 1; }
rm -f $errorfile
exec $bin -r $@
