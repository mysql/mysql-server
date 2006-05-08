#!/bin/bash
#
# export current working directory in a format suitable for sending to MySQL
# as a snapshot. also generates the actual snapshot and sends it to MySQL.

set -eu

die () {
  echo $*
  exit 1
}

if [ $# -ne 2 ] ; then
  die "Usage: export.sh revision-number-of-last-snapshot current-revision-number"
fi

set +u
if test -z $EDITOR; then
  die "\$EDITOR is not set"
fi
set -u

rm -rf to-mysql
mkdir -p to-mysql/storage/
svn log -v -r "$(($1 + 1)):BASE" > to-mysql/log
svn export -q . to-mysql/storage/innobase
cd to-mysql

mkdir -p sql mysql-test/t mysql-test/r mysql-test/include
cd storage/innobase

mv handler/* ../../sql
rmdir handler

mv mysql-test/*.test mysql-test/*.opt ../../mysql-test/t
mv mysql-test/*.result ../../mysql-test/r
mv mysql-test/*.inc ../../mysql-test/include
rmdir mysql-test

rm setup.sh export.sh revert_gen.sh compile-innodb-debug compile-innodb

cd ../..
$EDITOR log
cd ..

fname="innodb-5.1-ss$2.tar.gz"

rm -f $fname
tar czf $fname to-mysql
scp $fname mysql:snapshots
rm $fname
rm -rf to-mysql

echo "Sent $fname to MySQL"
