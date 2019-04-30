CREATE TABLE t1 (f1 INT);
CREATE FUNCTION bug27563() RETURNS INT(11)
DETERMINISTIC
BEGIN
DECLARE CONTINUE HANDLER FOR SQLSTATE '70100' SET @a:= 'killed';
DECLARE CONTINUE HANDLER FOR SQLEXCEPTION SET @a:= 'exception';
SET DEBUG_SYNC= 'now SIGNAL in_sync WAIT_FOR kill';
RETURN 1;
END|
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
INSERT INTO t1 VALUES (bug27563());
SET DEBUG_SYNC= 'now WAIT_FOR in_sync';
KILL QUERY @id;
ERROR 70100: Query execution was interrupted
SELECT * FROM t1;
f1
SET DEBUG_SYNC = 'RESET';
INSERT INTO t1 VALUES(0);
UPDATE t1 SET f1= bug27563();
SET DEBUG_SYNC= 'now WAIT_FOR in_sync';
KILL QUERY @id;
ERROR 70100: Query execution was interrupted
SELECT * FROM t1;
f1
0
SET DEBUG_SYNC = 'RESET';
INSERT INTO t1 VALUES(1);
DELETE FROM t1 WHERE bug27563() IS NULL;
SET DEBUG_SYNC= 'now WAIT_FOR in_sync';
KILL QUERY @id;
ERROR 70100: Query execution was interrupted
SELECT * FROM t1;
f1
0
1
SET DEBUG_SYNC = 'RESET';
SELECT * FROM t1 WHERE f1= bug27563();
SET DEBUG_SYNC= 'now WAIT_FOR in_sync';
KILL QUERY @id;
ERROR 70100: Query execution was interrupted
SELECT * FROM t1;
f1
0
1
SET DEBUG_SYNC = 'RESET';
DROP FUNCTION bug27563;
CREATE TABLE t2 (f2 INT) engine myisam;
CREATE TRIGGER trg27563 BEFORE INSERT ON t1 FOR EACH ROW
BEGIN
DECLARE CONTINUE HANDLER FOR SQLSTATE '70100' SET @a:= 'killed';
DECLARE CONTINUE HANDLER FOR SQLEXCEPTION SET @a:= 'exception';
INSERT INTO t2 VALUES(0);
SET DEBUG_SYNC= 'now SIGNAL in_sync WAIT_FOR kill';
INSERT INTO t2 VALUES(1);
END|
INSERT INTO t1 VALUES(2),(3);
SET DEBUG_SYNC= 'now WAIT_FOR in_sync';
KILL QUERY @id;
ERROR 70100: Query execution was interrupted
SELECT * FROM t1;
f1
0
1
SELECT * FROM t2;
f2
0
SET DEBUG_SYNC = 'RESET';
DROP TABLE t1, t2;
#
# Additional test for WL#3726 "DDL locking for all metadata objects"
# Check that DDL and DML statements waiting for metadata locks can
# be killed. Note that we don't cover all situations here since it
# can be tricky to write test case for some of them (e.g. REPAIR or
# ALTER and other statements under LOCK TABLES).
#
create table t1 (i int primary key) engine myisam;
# Test for RENAME TABLE
# Switching to connection 'blocker'
lock table t1 read;
# Switching to connection 'ddl'
rename table t1 to t2;
# Switching to connection 'default'
kill query ID;
# Switching to connection 'ddl'
ERROR 70100: Query execution was interrupted
# Test for DROP TABLE
drop table t1;
# Switching to connection 'default'
kill query ID;
# Switching to connection 'ddl'
ERROR 70100: Query execution was interrupted
# Test for CREATE TRIGGER
create trigger t1_bi before insert on t1 for each row set @a:=1;
# Switching to connection 'default'
kill query ID;
# Switching to connection 'ddl'
ERROR 70100: Query execution was interrupted
#
# Tests for various kinds of ALTER TABLE
#
# Full-blown ALTER which should copy table
alter table t1 add column j int;
# Switching to connection 'default'
kill query ID;
# Switching to connection 'ddl'
ERROR 70100: Query execution was interrupted
# Two kinds of simple ALTER
alter table t1 rename to t2;
# Switching to connection 'default'
kill query ID;
# Switching to connection 'ddl'
ERROR 70100: Query execution was interrupted
alter table t1 disable keys;
# Switching to connection 'default'
kill query ID;
# Switching to connection 'ddl'
ERROR 70100: Query execution was interrupted
# Fast ALTER
alter table t1 alter column i set default 100;
# Switching to connection 'default'
kill query ID;
# Switching to connection 'ddl'
ERROR 70100: Query execution was interrupted
# Special case which is triggered only for MERGE tables.
# Switching to connection 'blocker'
unlock tables;
create table t2 (i int primary key) engine=merge union=(t1);
lock tables t2 read;
# Switching to connection 'ddl'
alter table t2 alter column i set default 100;
# Switching to connection 'default'
kill query ID;
# Switching to connection 'ddl'
ERROR 70100: Query execution was interrupted
# Test for DML waiting for meta-data lock
# Switching to connection 'blocker'
unlock tables;
lock tables t1 read;
# Switching to connection 'ddl'
truncate table t1;
# Switching to connection 'dml'
insert into t1 values (1);
# Switching to connection 'default'
kill query ID2;
# Switching to connection 'dml'
ERROR 70100: Query execution was interrupted
# Switching to connection 'blocker'
unlock tables;
# Switching to connection 'ddl'
# Test for DML waiting for tables to be flushed
# Switching to connection 'blocker'
lock tables t1 read;
# Switching to connection 'ddl'
# Let us mark locked table t1 as old
flush tables;
# Switching to connection 'dml'
select * from t1;
# Switching to connection 'default'
kill query ID2;
# Switching to connection 'dml'
ERROR 70100: Query execution was interrupted
# Switching to connection 'blocker'
unlock tables;
# Switching to connection 'ddl'
# Cleanup.
# Switching to connection 'default'
drop table t1;
drop table t2;
