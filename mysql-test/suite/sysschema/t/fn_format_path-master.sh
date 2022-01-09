#!/bin/sh

mkdir $MYSQLTEST_VARDIR/tmp/innodb
cp $MYSQLTEST_VARDIR/mysqld.1/data/ibdata1 $MYSQLTEST_VARDIR/tmp/innodb
cp $MYSQLTEST_VARDIR/mysqld.1/data/ib_buffer_pool $MYSQLTEST_VARDIR/tmp/innodb
mkdir $MYSQLTEST_VARDIR/tmp/innodb_logs
mkdir $MYSQLTEST_VARDIR/tmp/innodb_logs/#innodb_redo
cp "$MYSQLTEST_VARDIR/mysqld.1/data/#innodb_redo/#ib_redo"* $MYSQLTEST_VARDIR/tmp/innodb_logs/#innodb_redo
mkdir $MYSQLTEST_VARDIR/tmp/innodb_undo