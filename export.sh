#!/bin/bash
#
# export current working directory in a format suitable for sending to
# MySQL as a snapshot.

set -eu

if [ $# -ne 1 ] ; then
  echo "Usage: export.sh revision-number-of-last-snapshot"
  exit 1
fi

rm -rf to-mysql
mkdir -p to-mysql/storage/
svn log -v -r "$1:BASE" > to-mysql/log
svn export . to-mysql/storage/innobase
cd to-mysql

mkdir -p sql mysql-test/t mysql-test/r mysql-test/include
cd storage/innobase

mv handler/* ../../sql
rmdir handler

mv mysql-test/*.test mysql-test/*.opt ../../mysql-test/t
mv mysql-test/*.result ../../mysql-test/r
mv mysql-test/*.inc ../../mysql-test/include
rmdir mysql-test

rm setup.sh export.sh compile-innodb-debug

