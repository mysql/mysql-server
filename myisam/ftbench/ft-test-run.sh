#!/bin/sh

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
OPTS="--no-defaults --socket=$SOCK --character-sets-dir=$ROOT/sql/share/charsets"

#  --ft_min_word_len=#
#  --ft_max_word_len=#
#  --ft_max_word_len_for_sort=# 
#  --ft_stopword_file=name 
#  --key_buffer_size=#

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

for batch in t/BEST t/* ; do
  A=`ls $batch/*.out`
  [ ! -d $batch -o -n "$A" ] && continue
  rm -f $H
  ln -s $BASE/$batch/ftdefs.h $H
  touch $H
  OPTS="--defaults-file=$BASE/$batch/my.cnf --socket=$SOCK --character-sets-dir=$ROOT/sql/share/charsets"
  stop_myslqd
  rm -f $MYSQLD
  (cd $ROOT; gmake)

  for prog in $MYSQLD $MYSQL $MYSQLADMIN ; do
    if [ ! -x $prog ] ; then
      echo "No $prog"
      exit 1
    fi
  done

  rm -rf var 2>&1 >/dev/null
  mkdir var
  mkdir var/test

  $MYSQLD $OPTS --basedir=$BASE --skip-bdb --pid-file=$PID \
                --language=$ROOT/sql/share/english \
                --skip-grant-tables --skip-innodb \
                --skip-networking --tmpdir=$DATA &

  sleep 60
  $MYSQLADMIN $OPTS ping
  if [ $? != 0 ] ; then
    echo "$MYSQLD refused to start"
    exit 1
  fi
  for test in `cd data; echo *.test|sed "s/\.test//g"` ; do
    echo "test $batch/$test"
    $MYSQL $OPTS --skip-column-names test <data/$test.test >var/$test.eval
    echo "report $batch/$test"
    ./Ereport.pl var/$test.eval data/$test.relj > $batch/$test.out || exit
  done
  stop_myslqd
  rm -f $H
  echo "compare $batch"
  [ $batch -ef t/BEST ] || ./Ecompare.pl t/BEST $batch >> t/BEST/report.txt
done

