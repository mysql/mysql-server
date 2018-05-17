include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE PROCEDURE test.p1()
BEGIN
INSERT INTO test.t1 VALUES(2);
END|
CREATE PROCEDURE test.p2()
BEGIN
DROP TEMPORARY TABLE IF EXISTS test.t1;
CREATE TEMPORARY TABLE test.t1 (a int, PRIMARY KEY(a));
INSERT INTO test.t1 VALUES(1);
CALL test.p1();
END|
CALL test.p2();
SELECT * FROM test.t1 ORDER BY a;
a
1
2
include/sync_slave_sql_with_master.inc
show tables;
Tables_in_test
CREATE PROCEDURE test.p3()
BEGIN
INSERT INTO test.t2 VALUES(7);
END|
CREATE PROCEDURE test.p4()
BEGIN
DROP TABLE IF EXISTS test.t2;
CREATE TABLE test.t2 (a int, PRIMARY KEY(a));
INSERT INTO test.t2 VALUES(6);
CALL test.p3();
END|
CALL test.p4();
SELECT * FROM test.t2 ORDER BY a;
a
6
7
include/sync_slave_sql_with_master.inc
SELECT * FROM test.t2 ORDER BY a;
a
6
7
DROP PROCEDURE IF EXISTS test.p1;
DROP PROCEDURE IF EXISTS test.p2;
DROP PROCEDURE IF EXISTS test.p3;
DROP PROCEDURE IF EXISTS test.p4;
DROP TABLE IF EXISTS test.t1;
DROP TABLE IF EXISTS test.t2;
include/sync_slave_sql_with_master.inc
include/rpl_end.inc
