#!/bin/sh

mkdir $MYSQLTEST_VARDIR/tmp/innodb
cp $MYSQLTEST_VARDIR/mysqld.1/data/ibdata1 $MYSQLTEST_VARDIR/tmp/innodb
cp $MYSQLTEST_VARDIR/mysqld.1/data/ib_buffer_pool $MYSQLTEST_VARDIR/tmp/innodb
mkdir $MYSQLTEST_VARDIR/tmp/innodb_logs
cp $MYSQLTEST_VARDIR/mysqld.1/data/ib_logfile* $MYSQLTEST_VARDIR/tmp/innodb_logs
mkdir $MYSQLTEST_VARDIR/tmp/innodb_undo