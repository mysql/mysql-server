drop table if exists t1,t2;
create table t1 ( n int auto_increment primary key);
lock tables t1 write;
insert into t1 values(NULL);
unlock tables;
check table t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
lock tables t1 write, t1 as t0 read;
insert into t1 values(NULL);
unlock tables;
check table t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
lock tables t1 write, t1 as t0 read, t1 as t2 read;
insert into t1 values(NULL);
unlock tables;
check table t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
lock tables t1 write, t1 as t0 write, t1 as t2 read;
insert into t1 values(NULL);
unlock tables;
check table t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
lock tables t1 write, t1 as t0 write, t1 as t2 read, t1 as t3 read;
insert into t1 values(NULL);
unlock tables;
check table t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
lock tables t1 write, t1 as t0 write, t1 as t2 write;
insert into t1 values(NULL);
unlock tables;
check table t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
drop table t1;
CREATE TABLE t1 (a int);
CREATE TABLE t2 (a int);
lock tables t1 write,t1 as b write, t2 write, t2 as c read;
drop table t1,t2;
CREATE TABLE t1 (a int);
CREATE TABLE t2 (a int);
lock tables t1 write,t1 as b write, t2 write, t2 as c read;
drop table t2,t1;
unlock tables;
create temporary table t1(f1 int);
lock tables t1 write;
insert into t1 values (1);
show columns from t1;
Field	Type	Null	Key	Default	Extra
f1	int	YES		NULL	NULL
insert into t1 values(2);
drop table t1;
unlock tables;
#
# Bug#19988193 ASSERTION `(*TABLES)->REGINFO.LOCK_TYPE >= TL_READ'
# FAILED IN LOCK_EXTERNAL
#
CREATE TABLE t1(a INT);
CREATE PROCEDURE p1() CREATE VIEW v1 AS SELECT * FROM t1;

# Create trigger calling proc creating view, when view DOES NOT
# exist already
CREATE TRIGGER trg_p1_t1 AFTER INSERT ON t1 FOR EACH ROW CALL p1();

# Verify that it is possible to lock table
LOCK TABLES t1 WRITE;
UNLOCK TABLES;

# Fails, as expected
INSERT INTO t1 VALUES (1);
ERROR HY000: Explicit or implicit commit is not allowed in stored function or trigger.

# Make sure v1 already exists
CREATE VIEW v1 AS SELECT a+1 FROM t1;

# Verify that it is possible to lock table
LOCK TABLES t1 WRITE;
UNLOCK TABLES;

# Verify that we get the expected error when inserting into the table
INSERT INTO t1 VALUES (1);
ERROR HY000: Explicit or implicit commit is not allowed in stored function or trigger.

# Cleanup
DROP TRIGGER trg_p1_t1;
DROP PROCEDURE p1;
DROP VIEW v1;
DROP TABLE t1;
#
# Bug#21198646 ASSERTION FAILED: (*TABLES)->REGINFO.LOCK_TYPE >= TL_READ
# FILE LOCK.CC, LINE 356
#
CREATE TABLE t2(a INT);
# Create procedure p1 invoking RENAME TABLE
CREATE PROCEDURE p1() RENAME TABLE t2 TO t3;
# Create function f1 calling p1
CREATE FUNCTION f1() RETURNS INT BEGIN CALL p1(); RETURN 1; END $
# Invoke function f1 and verify that we get the expected error
SELECT f1();
ERROR HY000: Explicit or implicit commit is not allowed in stored function or trigger.
# Cleanup
DROP PROCEDURE p1;
DROP FUNCTION f1;
DROP TABLE t2;
