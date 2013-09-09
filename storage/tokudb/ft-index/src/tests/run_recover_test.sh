#!/usr/bin/env bash

set -e

test $# -ge 4

bin=$1; shift
envdir=$1; shift
tdbrecover=$1; shift
tdbdump=$1; shift

echo doing $bin
$bin --no-shutdown
rm -rf $envdir/recoverdir
mkdir $envdir/recoverdir
cp $envdir/tokudb.directory $envdir/recoverdir/
cp $envdir/tokudb.environment $envdir/recoverdir/
cp $envdir/tokudb.rollback $envdir/recoverdir/
cp $envdir/*.tokulog* $envdir/recoverdir/
echo doing recovery
$tdbrecover $envdir/recoverdir $envdir/recoverdir
echo dump and compare
$tdbdump -h $envdir foo.db >$envdir/foo.dump
$tdbdump -h $envdir/recoverdir foo.db >$envdir/recoverdir/foo.dump
diff -q $envdir/foo.dump $envdir/recoverdir/foo.dump
