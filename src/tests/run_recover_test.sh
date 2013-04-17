#!/bin/bash

set -e

test $# -ge 4

bin=$1; shift
envdir=$1; shift
tdbrecover=$1; shift
tdbdump=$1; shift

echo doing $bin
$bin --no-shutdown
rm -rf $envdir.recover
mkdir $envdir.recover
cp $envdir/tokudb.directory $envdir.recover/
cp $envdir/tokudb.environment $envdir.recover/
cp $envdir/tokudb.rollback $envdir.recover/
cp $envdir/*.tokulog* $envdir.recover/
echo doing recovery
$tdbrecover $envdir.recover $envdir.recover
echo dump and compare
$tdbdump -h $envdir foo.db >$envdir/foo.dump
$tdbdump -h $envdir.recover foo.db >$envdir.recover/foo.dump
diff -q $envdir/foo.dump $envdir.recover/foo.dump
