#!/bin/sh

# Copyright (c) 2002, 2022, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

if [ ! -x ./ft-test-run.sh ] ; then
  echo "Usage: ./ft-test-run.sh"
  exit 1
fi

BASE=`pwd`
DATA=$BASE/var
ROOT=`cd ../..; pwd`
MYSQLD=$ROOT/sql/mysqld
MYSQL=$ROOT/client/mysql
MYSQLADMIN=$ROOT/client/mysqladmin
SOCK=$DATA/mysql.sock
PID=$DATA/mysql.pid
H=../ftdefs.h
OPTS="--no-defaults --socket=$SOCK --character-sets-dir=$ROOT/share/charsets"
DELAY=10

stop_myslqd()
{
  [ -S $SOCK ] && $MYSQLADMIN $OPTS shutdown
  [ -f $PID ] && kill `cat $PID` && sleep 15 && [ -f $PID ] && kill -9 `cat $PID`
}

if [ ! -d t/BEST ] ; then
  echo "No ./t/BEST directory! Aborting..."
  exit 1
fi
rm -f t/BEST/report.txt
if [ -w $H ] ; then
  echo "$H is writeable! Aborting..."
  exit 1
fi

stop_myslqd
rm -rf var > /dev/null 2>&1
mkdir var
mkdir var/test

for batch in t/* ; do
  [ ! -d $batch ] && continue
  [ $batch -ef t/BEST -a $batch != t/BEST ] && continue

  rm -rf var/test/* > /dev/null 2>&1
  rm -f $H
  if [ -f $BASE/$batch/ftdefs.h ] ; then
    cat $BASE/$batch/ftdefs.h > $H
    chmod a-wx $H
  else
    bk get -q $H
  fi
  OPTS="--defaults-file=$BASE/$batch/my.cnf --socket=$SOCK --character-sets-dir=$ROOT/share/charsets"
  stop_myslqd
  rm -f $MYSQLD
  echo "building $batch"
  echo "============== $batch ===============" >> var/ft_test.log
  (cd $ROOT; gmake) >> var/ft_test.log 2>&1

  for prog in $MYSQLD $MYSQL $MYSQLADMIN ; do
    if [ ! -x $prog ] ; then
      echo "build failed: no $prog"
      exit 1
    fi
  done

  echo "=====================================" >> var/ft_test.log
  $MYSQLD $OPTS --basedir=$BASE --pid-file=$PID \
                --language=$ROOT/share/english \
                --skip-grant-tables --skip-innodb \
                --skip-networking --tmpdir=$DATA >> var/ft_test.log 2>&1 &

  sleep $DELAY
  $MYSQLADMIN $OPTS ping
  if [ $? != 0 ] ; then
    echo "$MYSQLD refused to start"
    exit 1
  fi
  for test in `cd data; echo *.r|sed "s/\.r//g"` ; do
    if [ -f $batch/$test.out ] ; then
      echo "skipping $batch/$test.out"
      continue
    fi
    echo "testing $batch/$test"
    FT_MODE=`cat $batch/ft_mode 2>/dev/null`
    ./Ecreate.pl $test "$FT_MODE" | $MYSQL $OPTS --skip-column-names test >var/$test.eval
    echo "reporting $batch/$test"
    ./Ereport.pl var/$test.eval data/$test.r > $batch/$test.out || exit
  done
  stop_myslqd
  rm -f $H
  bk get -q $H
  if [ ! $batch -ef t/BEST ] ; then
    echo "comparing $batch"
    ./Ecompare.pl t/BEST $batch >> t/BEST/report.txt
  fi
done

