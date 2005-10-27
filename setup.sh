#!/bin/bash
#
# Prepare the MySQL source code tree for building
# with checked-out InnoDB Subversion directory.

# This script assumes that the MySQL tree is at .. and that . = ../innodb

set -eu

TARGETDIR=../storage/innobase

rm -fr "$TARGETDIR"
mkdir "$TARGETDIR"

# create the directories
for dir in */
do
  case "$dir" in
      handler/) ;;
      mysql-test/) ;;
      *.svn*) ;;
      *to-mysql*) ;;
      *) mkdir "$TARGETDIR/$dir" ;;
  esac
done

# create the symlinks to files
cd "$TARGETDIR"
for dir in */
do
  cd "$dir"
  ln -s ../../../innodb/"$dir"* .
  cd ..
done
for file in configure.in Makefile.am
do
  ln -s ../../innodb/"$file" .
done

cd ..
ln -sf ../innodb/handler/ha_innodb.h ../sql/
ln -sf ../innodb/handler/ha_innodb.cc ../sql/

cd ../mysql-test/t
ln -sf ../../innodb/mysql-test/*.test ../../innodb/mysql-test/*.opt ./
ln -sf ../../innodb/mysql-test/*.result ../r/
ln -sf ../../innodb/mysql-test/*.inc ../include/
