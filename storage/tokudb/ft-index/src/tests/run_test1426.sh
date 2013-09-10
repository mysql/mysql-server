#!/usr/bin/env bash

set -e

test $# -ge 4

tdbbin=$1; shift
bdbbin=$1; shift
tdbenv=$1; shift
bdbenv=$1; shift
tdbdump=$1; shift
bdbdump=$1; shift

TOKU_TEST_FILENAME=$bdbenv $bdbbin
$bdbdump -p -h $bdbenv main > dump.bdb.1426

TOKU_TEST_FILENAME=$tdbenv $tdbbin
$tdbdump -x -p -h $tdbenv main > dump.tdb.1426
diff -I db_pagesize=4096 dump.bdb.1426 dump.tdb.1426
