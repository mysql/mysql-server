include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CHANGE REPLICATION FILTER replicate_do_db=(db1);
ERROR HY000: This operation cannot be performed with a running slave sql thread; run STOP SLAVE SQL_THREAD FOR CHANNEL '' first.
CHANGE REPLICATION FILTER replicate_ignore_db=(db2);
ERROR HY000: This operation cannot be performed with a running slave sql thread; run STOP SLAVE SQL_THREAD FOR CHANNEL '' first.
include/stop_slave.inc
CHANGE REPLICATION FILTER replicate_do_db=();
CHANGE REPLICATION FILTER replicate_ignore_db=();
CHANGE REPLICATION FILTER replicate_do_db=(db1);
CHANGE REPLICATION FILTER replicate_ignore_db=(db2);
include/start_slave.inc
CREATE DATABASE db1;
CREATE DATABASE db2;
CREATE DATABASE db3;
USE db1;
CREATE TABLE t1 (a INT);
USE db2;
CREATE TABLE t2 (a INT);
USE db3;
CREATE TABLE t3 (a INT);
USE db1;
INSERT INTO t1 VALUES (1);
USE db2;
INSERT INTO t2 VALUES (2);
USE db3;
INSERT INTO t3 VALUES (3);
[on slave]
SHOW DATABASES LIKE 'db%';
Database (db%)
db1
SHOW TABLES IN db1 LIKE 't%';
Tables_in_db1 (t%)
t1
[on master]
[on master]
DROP DATABASE IF EXISTS db1;
DROP DATABASE IF EXISTS db2;
DROP DATABASE IF EXISTS db3;
include/rpl_end.inc
CHANGE REPLICATION FILTER replicate_do_db=();
CHANGE REPLICATION FILTER replicate_ignore_db=();
