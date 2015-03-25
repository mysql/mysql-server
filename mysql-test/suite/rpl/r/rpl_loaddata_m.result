include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
drop database if exists mysqltest;
USE test;
CREATE TABLE t1(a INT, b INT, UNIQUE(b));
LOAD DATA INFILE '../../std_data/rpl_loaddata.dat' INTO TABLE test.t1;
SELECT COUNT(*) FROM test.t1;
COUNT(*)
2
CREATE DATABASE mysqltest;
USE mysqltest;
CREATE TABLE t1(a INT, b INT, UNIQUE(b));
LOAD DATA INFILE '../../std_data/rpl_loaddata.dat' INTO TABLE mysqltest.t1;
SELECT COUNT(*) FROM mysqltest.t1;
COUNT(*)
2
include/sync_slave_sql_with_master.inc
SHOW DATABASES;
Database
information_schema
mtr
mysql
mysqltest
performance_schema
sys
test
USE test;
SHOW TABLES;
Tables_in_test
USE mysqltest;
SHOW TABLES;
Tables_in_mysqltest
t1
SELECT COUNT(*) FROM mysqltest.t1;
COUNT(*)
2
DROP DATABASE mysqltest;
DROP TABLE IF EXISTS test.t1;
include/sync_slave_sql_with_master.inc
include/rpl_end.inc
