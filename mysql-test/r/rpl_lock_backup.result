#
# WL#9451 -- Backup Log
#
include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE TABLE t1 (a INT);
CREATE USER 'testuser1'@'localhost';
GRANT all ON *.* TO 'testuser1'@'localhost';
connect  con1, localhost, testuser1,,;
SET lock_wait_timeout= 1;
SET autocommit= 0;
connection default;
LOCK INSTANCE FOR BACKUP;
connection con1;
CREATE DATABASE testdb;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
CREATE TABLESPACE testtablespace ADD DATAFILE 'ts.ibd' ENGINE=InnoDB;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
CREATE TABLE testtable_1 (c1 int, c2 varchar(10));
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
CREATE TEMPORARY TABLE temptable_1 (tt1 int);
INSERT INTO temptable_1 SELECT * FROM t1;
SELECT * FROM temptable_1;
tt1
connection default;
UNLOCK INSTANCE;
connection con1;
CREATE DATABASE testdb;
CREATE TABLESPACE testtablespace ADD DATAFILE 'ts.ibd' ENGINE=InnoDB;
CREATE TABLE testtable_1 (c1 int, c2 varchar(10)) TABLESPACE testtablespace;
connection default;
LOCK INSTANCE FOR BACKUP;
connection con1;
INSERT INTO testtable_1 VALUES(1,'aaa'),(2,'bbbbbb'),(3,'cccccccc');
INSERT INTO testtable_1 VALUES(4,'ddd'),(5,'eeeee'),(3,'fffffff');
UPDATE testtable_1 SET c1=11, c2='yyy' WHERE c1=1;
DELETE FROM testtable_1 WHERE c2 LIKE "aa";
ALTER DATABASE testdb CHARACTER SET 'latin1';
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
ALTER TABLE testtable_1 ADD INDEX i2(c2);
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
connection default;
UNLOCK INSTANCE;
connection con1;
DROP DATABASE testdb;
DROP TABLE testtable_1;
DROP TABLESPACE testtablespace;
connection default;
disconnect con1;
DROP USER 'testuser1'@'localhost';
DROP TABLE t1;
include/rpl_end.inc
