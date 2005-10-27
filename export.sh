#!/bin/bash
#
# export current working directory in a format suitable for sending to
# MySQL as a snapshot.

rm -rf to-mysql
svn export . to-mysql
cd to-mysql

mkdir innobase
mv * innobase
mkdir -p sql mysql-test/t mysql-test/r mysql-test/include
cd innobase

mv handler/* ../sql
rmdir handler

mv mysql-test/*.test mysql-test/*.opt ../mysql-test/t
mv mysql-test/*.result ../mysql-test/r
mv mysql-test/*.inc ../mysql-test/include
rmdir mysql-test

rm setup.sh export.sh
