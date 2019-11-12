#
# WL#9451 -- Backup Log
#
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
CREATE VIEW testview_1 AS SELECT c2 FROM testtable_1;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
CREATE INDEX testindex_1 ON testtable_1(c1);
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
CREATE SERVER s FOREIGN DATA WRAPPER mysql OPTIONS (DATABASE 'test');
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
CREATE PROCEDURE testproc_1()
BEGIN
SELECT * FROM testtable_1;
END//
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
CREATE EVENT testevent_1 ON SCHEDULE EVERY 10 SECOND DO SELECT 1;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
CREATE TRIGGER testtrigger_1 BEFORE INSERT ON testtable_1 FOR EACH ROW SET @a:=1;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
INSERT INTO t1 VALUES(1),(2),(3);
CREATE TEMPORARY TABLE temptable_1 (tt1 int);
INSERT INTO temptable_1 SELECT * FROM t1;
SELECT * FROM temptable_1;
tt1
1
2
3
CREATE RESOURCE GROUP rg1 TYPE=USER;
Got one of the listed errors
connection default;
UNLOCK INSTANCE;
connection con1;
CREATE DATABASE testdb;
CREATE TABLESPACE testtablespace ADD DATAFILE 'ts.ibd' ENGINE=InnoDB;
CREATE TABLE testtable_1 (c1 int, c2 varchar(10)) TABLESPACE testtablespace;
CREATE TABLE testtable_3 (c1 int, c2 varchar(10)) PARTITION BY RANGE (c1)(
PARTITION p0 VALUES LESS THAN (10),
PARTITION p2 VALUES LESS THAN (100),
PARTITION p3 VALUES LESS THAN (1000));
INSERT testtable_3 VALUES (1,'a'),(11,'b'),(101,'c');
ALTER TABLE testtable_3 ADD PARTITION (PARTITION p4 VALUES LESS THAN (5000));
CREATE VIEW testview_1 AS SELECT c2 FROM testtable_1;
CREATE INDEX testindex_1 ON testtable_1(c1);
CREATE PROCEDURE testproc_1()
BEGIN
SELECT * FROM testtable_1;
END//
CREATE EVENT testevent_1 ON SCHEDULE EVERY 10 SECOND DO SELECT 1;
CREATE TRIGGER testtrigger_1 BEFORE INSERT ON testtable_1 FOR EACH ROW SET @a:=1;
CREATE SERVER s FOREIGN DATA WRAPPER mysql OPTIONS (DATABASE 'test');
connection default;
LOCK INSTANCE FOR BACKUP;
LOCK INSTANCE FOR BACKUP;
connection con1;
INSERT INTO testtable_1 VALUES(1,'aaa'),(2,'bbbbbb'),(3,'cccccccc');
INSERT INTO testtable_1 VALUES(4,'ddd'),(5,'eeeee'),(3,'fffffff');
UPDATE testtable_1 SET c1=11, c2='yyy' WHERE c1=1;
DELETE FROM testtable_1 WHERE c2 LIKE "aa";
call testproc_1;
c1	c2
11	yyy
2	bbbbbb
3	cccccccc
4	ddd
5	eeeee
3	fffffff
INSERT testtable_3 VALUES (4999,'aa');
ALTER DATABASE testdb CHARACTER SET 'latin1';
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
ALTER TABLE testtable_1 ADD INDEX i2(c2);
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
ALTER VIEW testview_1 AS SELECT c1 FROM testtable_1;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
ALTER PROCEDURE testproc_1 COMMENT 'procedure';
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
ALTER EVENT testevent_1 ON SCHEDULE EVERY 100 SECOND DO SELECT 1;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
ALTER SERVER s OPTIONS (USER 'sally');
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
ALTER TABLE testtable_1 ALGORITHM=INPLACE, ADD COLUMN c3 VARCHAR(255);
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
ALTER TABLE testtable_3 ADD PARTITION (PARTITION p5 VALUES LESS THAN (10000));
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
connection default;
UNLOCK INSTANCE;
UNLOCK INSTANCE;
connection con1;
ALTER DATABASE testdb CHARACTER SET 'latin1';
ALTER TABLE testtable_1 ADD INDEX i2(c2);
ALTER VIEW testview_1 AS SELECT c1 FROM testtable_1;
ALTER PROCEDURE testproc_1 COMMENT 'procedure';
ALTER EVENT testevent_1 ON SCHEDULE EVERY 100 SECOND DO SELECT 1;
ALTER SERVER s OPTIONS (USER 'sally');
ALTER TABLE testtable_1 ALGORITHM=INPLACE, ADD COLUMN c3 VARCHAR(255);
ALTER TABLE testtable_3 ADD PARTITION (PARTITION p5 VALUES LESS THAN (10000));
INSERT testtable_3 VALUES (9999,'aa');
connection default;
connection con1;
DROP DATABASE testdb;
DROP TABLE testtable_1;
DROP TABLE testtable_3;
DROP VIEW testview_1;
DROP TABLESPACE testtablespace;
DROP PROCEDURE testproc_1;
DROP EVENT testevent_1;
DROP SERVER s;
connection default;
disconnect con1;
LOCK INSTANCE FOR BACKUP;
# Restart server
# restart
CREATE TABLE t2 (c1 int);
DROP USER 'testuser1'@'localhost';
DROP TABLE t1, t2;
