# Test of mysqlhotcopy (perl script)
# Author: Horst Hunger
# Created: 2010-05-10

--source include/not_windows.inc
--source include/not_embedded.inc

if ($MYSQLHOTCOPY)
{
  die due to missing mysqlhotcopy tool;
}

let $MYSQLD_DATADIR= `SELECT @@datadir`;
--disable_warnings
DROP DATABASE IF EXISTS hotcopy_test;
--enable_warnings
CREATE DATABASE hotcopy_test;
USE hotcopy_test;
eval CREATE TABLE t1 (c1 int, c2 varchar(20)) ENGINE=$engine;
eval CREATE TABLE t2 (c1 int, c2 varchar(20)) ENGINE=$engine;
eval CREATE TABLE t3 (c1 int, c2 varchar(20)) ENGINE=$engine;

INSERT INTO t1 VALUES (1,'aaaaaaaaaaaaaaaaaaaa'),(2, 'bbbbbbbbbbbbbbbbbbbbbbb');
INSERT INTO t2 VALUES (1,'aaaaaaaaaaaaaaaaaaaa'),(2, 'bbbbbbbbbbbbbbbbbbbbbbb');
INSERT INTO t3 VALUES (1,'aaaaaaaaaaaaaaaaaaaa'),(2, 'bbbbbbbbbbbbbbbbbbbbbbb');

--replace_result $MYSQLD_DATADIR MYSQLD_DATADIR
--list_files $MYSQLD_DATADIR/hotcopy_test

# backup into another database in the same directory
--replace_result $MASTER_MYSOCK MASTER_MYSOCK
--exec $MYSQLHOTCOPY --quiet -S $MASTER_MYSOCK -u root hotcopy_test hotcopy_save

--replace_result $MYSQLD_DATADIR MYSQLD_DATADIR
--list_files $MYSQLD_DATADIR/hotcopy_save

USE hotcopy_save;
SELECT * FROM t1;
SELECT * FROM t2;
SELECT * FROM t3;

# restore data into the original database with mysqlhotcopy
if(`SELECT engine= 'MyISAM' FROM information_schema.tables WHERE table_name='t1'`)
{
USE hotcopy_test;
DELETE FROM t1;
SELECT * FROM t1;

--replace_result $MASTER_MYSOCK MASTER_MYSOCK
--exec $MYSQLHOTCOPY --quiet --addtodest -S $MASTER_MYSOCK -u root hotcopy_save hotcopy_test

USE hotcopy_save;
SELECT * FROM t1;
SELECT * FROM t2;
SELECT * FROM t3;
}

USE hotcopy_test;
DROP TABLE t2;
--replace_result $MYSQLD_DATADIR MYSQLD_DATADIR
--list_files $MYSQLD_DATADIR/hotcopy_test

--replace_result $MASTER_MYSOCK MASTER_MYSOCK
--exec $MYSQLHOTCOPY --quiet --addtodest -S $MASTER_MYSOCK -u root hotcopy_save hotcopy_test

FLUSH TABLES;
SELECT * FROM t1;
SELECT * FROM t2;
SELECT * FROM t3;

# backup of db into a directory
USE hotcopy_test;
--replace_result $MASTER_MYSOCK MASTER_MYSOCK $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--exec $MYSQLHOTCOPY --quiet -S $MASTER_MYSOCK -u root hotcopy_test $MYSQLTEST_VARDIR/tmp
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--list_files $MYSQLTEST_VARDIR/tmp/hotcopy_test
#--exec rm -rf $MYSQLTEST_VARDIR/tmp/hotcopy_test
--remove_files_wildcard $MYSQLTEST_VARDIR/tmp/hotcopy_test *
--rmdir $MYSQLTEST_VARDIR/tmp/hotcopy_test

# backup without full index files
# reproduction of bug#53556, "--list_files" shows MYI files, which is wrong.
DROP DATABASE hotcopy_save;
--replace_result $MASTER_MYSOCK MASTER_MYSOCK
--exec $MYSQLHOTCOPY --quiet --noindices -S $MASTER_MYSOCK -u root hotcopy_test hotcopy_save
--replace_result $MYSQLD_DATADIR MYSQLD_DATADIR
--list_files $MYSQLD_DATADIR/hotcopy_save

# test of option "allowold"
DROP DATABASE hotcopy_save;
--replace_result $MASTER_MYSOCK MASTER_MYSOCK
--exec $MYSQLHOTCOPY --quiet -S $MASTER_MYSOCK -u root hotcopy_test hotcopy_save
--replace_result $MYSQLD_DATADIR MYSQLD_DATADIR
--list_files $MYSQLD_DATADIR/hotcopy_save
--replace_result $MASTER_MYSOCK MASTER_MYSOCK
--error 9,2304
--exec $MYSQLHOTCOPY --quiet -S $MASTER_MYSOCK -u root hotcopy_test hotcopy_save
--replace_result $MASTER_MYSOCK MASTER_MYSOCK
--exec $MYSQLHOTCOPY --quiet --allowold -S $MASTER_MYSOCK -u root hotcopy_test hotcopy_save
--replace_result $MYSQLD_DATADIR MYSQLD_DATADIR
--list_files $MYSQLD_DATADIR/hotcopy_save

# test of option "keepold"
--replace_result $MASTER_MYSOCK MASTER_MYSOCK
--exec $MYSQLHOTCOPY --quiet --keepold -S $MASTER_MYSOCK -u root hotcopy_test hotcopy_save
--replace_result $MYSQLD_DATADIR MYSQLD_DATADIR
--list_files $MYSQLD_DATADIR/hotcopy_save_old
--replace_result $MYSQLD_DATADIR MYSQLD_DATADIR
--list_files $MYSQLD_DATADIR/hotcopy_save

# test of option "suffix"
--replace_result $MASTER_MYSOCK MASTER_MYSOCK
--exec $MYSQLHOTCOPY --quiet --suffix=_cpy -S $MASTER_MYSOCK -u root hotcopy_test
--replace_result $MYSQLD_DATADIR MYSQLD_DATADIR
--list_files $MYSQLD_DATADIR/hotcopy_test_cpy
DROP DATABASE hotcopy_test_cpy;

DROP DATABASE hotcopy_test;
DROP DATABASE hotcopy_save;
DROP DATABASE hotcopy_save_old;

