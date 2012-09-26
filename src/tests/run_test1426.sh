#!/usr/bin/env bash

set -e

test $# -ge 4

tdbbin=$1; shift
bdbbin=$1; shift
tdbdump=$1; shift
bdbdump=$1; shift

$bdbbin
$bdbdump -p -h dir.test1426.bdb main > dump.bdb.1426

$tdbbin
$tdbdump -x -p -h dir.test1426.tdb main > dump.tdb.1426
diff -I db_pagesize=4096 dump.bdb.1426 dump.tdb.1426
