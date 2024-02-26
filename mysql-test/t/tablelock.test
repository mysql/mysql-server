
#
# Test of lock tables
#

--disable_warnings
drop table if exists t1,t2;
--enable_warnings

create table t1 ( n int auto_increment primary key);
lock tables t1 write;
insert into t1 values(NULL);
unlock tables;
check table t1;
lock tables t1 write, t1 as t0 read;
insert into t1 values(NULL);
unlock tables;
check table t1;
lock tables t1 write, t1 as t0 read, t1 as t2 read;
insert into t1 values(NULL);
unlock tables;
check table t1;
lock tables t1 write, t1 as t0 write, t1 as t2 read;
insert into t1 values(NULL);
unlock tables;
check table t1;
lock tables t1 write, t1 as t0 write, t1 as t2 read, t1 as t3 read;
insert into t1 values(NULL);
unlock tables;
check table t1;
lock tables t1 write, t1 as t0 write, t1 as t2 write;
insert into t1 values(NULL);
unlock tables;
check table t1;
drop table t1;

#
# Test of locking and delete of files
#

CREATE TABLE t1 (a int);
CREATE TABLE t2 (a int);
lock tables t1 write,t1 as b write, t2 write, t2 as c read;
drop table t1,t2;

CREATE TABLE t1 (a int);
CREATE TABLE t2 (a int);
lock tables t1 write,t1 as b write, t2 write, t2 as c read;
drop table t2,t1;
unlock tables;

# End of 4.1 tests

#
# Bug#23588 SHOW COLUMNS on a temporary table causes locking issues
#
create temporary table t1(f1 int);
lock tables t1 write;
insert into t1 values (1);
show columns from t1;
insert into t1 values(2);
drop table t1;
unlock tables;

# End of 5.0 tests

--echo #
--echo # Bug#19988193 ASSERTION `(*TABLES)->REGINFO.LOCK_TYPE >= TL_READ'
--echo # FAILED IN LOCK_EXTERNAL
--echo #

CREATE TABLE t1(a INT);
CREATE PROCEDURE p1() CREATE VIEW v1 AS SELECT * FROM t1;
--echo
--echo # Create trigger calling proc creating view, when view DOES NOT
--echo # exist already
CREATE TRIGGER trg_p1_t1 AFTER INSERT ON t1 FOR EACH ROW CALL p1();
--echo
--echo # Verify that it is possible to lock table
LOCK TABLES t1 WRITE;
UNLOCK TABLES;
--echo
--echo # Fails, as expected
--error ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG
INSERT INTO t1 VALUES (1);
--echo
--echo # Make sure v1 already exists
CREATE VIEW v1 AS SELECT a+1 FROM t1;
--echo
--echo # Verify that it is possible to lock table
LOCK TABLES t1 WRITE;
UNLOCK TABLES;
--echo
--echo # Verify that we get the expected error when inserting into the table
--error ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG
INSERT INTO t1 VALUES (1);
--echo
--echo # Cleanup
DROP TRIGGER trg_p1_t1;
DROP PROCEDURE p1;
DROP VIEW v1;
DROP TABLE t1;


--echo #
--echo # Bug#21198646 ASSERTION FAILED: (*TABLES)->REGINFO.LOCK_TYPE >= TL_READ
--echo # FILE LOCK.CC, LINE 356
--echo #

CREATE TABLE t2(a INT);

--echo # Create procedure p1 invoking RENAME TABLE
CREATE PROCEDURE p1() RENAME TABLE t2 TO t3;

--echo # Create function f1 calling p1
DELIMITER $;
CREATE FUNCTION f1() RETURNS INT BEGIN CALL p1(); RETURN 1; END $
DELIMITER ;$

--echo # Invoke function f1 and verify that we get the expected error
--error ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG
SELECT f1();

--echo # Cleanup
DROP PROCEDURE p1;
DROP FUNCTION f1;
DROP TABLE t2;