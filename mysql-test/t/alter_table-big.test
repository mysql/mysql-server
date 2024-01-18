#
# Tests for various concurrency-related aspects of ALTER TABLE implemetation
#
# This test takes rather long time so let us run it only in --big-test mode
--source include/big_test.inc
# We are using some debug-only features in this test
--source include/have_debug.inc

# Also we are using SBR to check that statements are executed
# in proper order.
--source include/rpl/force_binlog_format_statement.inc

--source include/count_sessions.inc

#
# Additional coverage for the main ALTER TABLE case
#
# We should be sure that table being altered is properly
# locked during statement execution and in particular that
# no DDL or DML statement can sneak in and get access to
# the table when real operation has already taken place
# but this fact has not been noted in binary log yet.
--disable_warnings
drop table if exists t1, t2, t3;
--enable_warnings
connect (addconroot, localhost, root,,);
connect (addconroot2, localhost, root,,);
connection default;
create table t1 (i int);
# We are going to check that statements are logged in correct order
reset binary logs and gtids;
set debug_sync='alter_table_before_main_binlog SIGNAL parked WAIT_FOR go';
--send alter table t1 change i c char(10) default 'Test1';
connection addconroot;
# Wait until ALTER TABLE acquires metadata lock.
set debug_sync='now WAIT_FOR parked'; 
--send insert into t1 values ();
connection addconroot2;
# Wait until the above INSERT INTO t1 is blocked due to ALTER
let $wait_condition=
    select count(*) = 1 from information_schema.processlist
    where state = "Waiting for table metadata lock" and
          info = "insert into t1 values ()";
--source include/wait_condition.inc
# Resume ALTER execution.
set debug_sync='now SIGNAL go';
connection default;
--reap
connection addconroot;
--reap
connection default;
select * from t1;
set debug_sync='alter_table_before_main_binlog SIGNAL parked WAIT_FOR go';
--send alter table t1 change c vc varchar(100) default 'Test2';
connection addconroot;
# Wait until ALTER TABLE acquires metadata lock.
set debug_sync='now WAIT_FOR parked';
--send rename table t1 to t2;
connection addconroot2;
# Wait until the above RENAME TABLE is blocked due to ALTER
let $wait_condition=
    select count(*) = 1 from information_schema.processlist
    where state = "Waiting for table metadata lock" and
          info = "rename table t1 to t2";
--source include/wait_condition.inc
# Resume ALTER execution.
set debug_sync='now SIGNAL go';
connection default;
--reap
connection addconroot;
--reap
connection default;
drop table t2;
# And now tests for ALTER TABLE with RENAME clause. In this
# case target table name should be properly locked as well.
create table t1 (i int);
set debug_sync='alter_table_before_main_binlog SIGNAL parked WAIT_FOR go';
--send alter table t1 change i c char(10) default 'Test3', rename to t2;
connection addconroot;
# Wait until ALTER TABLE acquires metadata lock.
set debug_sync='now WAIT_FOR parked';
--send insert into t2 values();
connection addconroot2;
# Wait until the above INSERT INTO t2 is blocked due to ALTER
let $wait_condition=
    select count(*) = 1 from information_schema.processlist
    where state = "Waiting for table metadata lock" and
           info = "insert into t2 values()";
--source include/wait_condition.inc
# Resume ALTER execution.
set debug_sync='now SIGNAL go';
connection default;
--reap
connection addconroot;
--reap
connection default;
select * from t2;
--send alter table t2 change c vc varchar(100) default 'Test2', rename to t1;
connection addconroot;
connection default;
--reap
rename table t1 to t3;

disconnect addconroot;
disconnect addconroot2;
drop table t3;
set debug_sync='alter_table_before_main_binlog SIGNAL parked WAIT_FOR go';
set debug_sync='RESET';

# Check that all statements were logged in correct order
source include/rpl/deprecated/show_binlog_events.inc;


--echo End of 5.1 tests
--source include/rpl/restore_default_binlog_format.inc


--echo #
--echo # Additional coverage for WL#7743 "New data dictionary: changes
--echo # to DDL-related parts of SE API".
--echo #
--echo # Killed ALTER TABLE on temporary table sometimes led to assertion
--echo # failure on connection close.
--enable_connect_log
--connect (con1, localhost, root,,)
create temporary table t1 (i int) engine=innodb;
set debug= "+d,mysql_lock_tables_kill_query";
--error ER_QUERY_INTERRUPTED
alter table t1 add index (i);
set debug= "-d,mysql_lock_tables_kill_query";
--echo # The below disconnect should drop temporary table automagically.
--disconnect con1
--source include/wait_until_disconnected.inc
connection default;
--disable_connect_log


--echo #
--echo # Test coverage for new (since 8.0) behavior of ALTER TABLE RENAME
--echo # under LOCK TABLES.
--echo #

--enable_connect_log
connect (con1, localhost, root,,);
SET @old_lock_wait_timeout= @@lock_wait_timeout;
connection default;

--echo #
--echo # 1) Simple ALTER TABLE RENAME.
--echo #
--echo # 1.1) Successfull ALTER TABLE RENAME.
--echo #
CREATE TABLE t1 (i INT);
LOCK TABLES t1 WRITE;
ALTER TABLE t1 RENAME TO t2;
--echo # Table is available under new name under LOCK TABLES.
SELECT * FROM t2;

connection con1;
--echo # Access by new name from other connections should be blocked.
SET @@lock_wait_timeout= 1;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t2;
SET @@lock_wait_timeout= @old_lock_wait_timeout;
--echo # But not for old table name.
--error ER_NO_SUCH_TABLE
SELECT * FROM t1;

connection default;
UNLOCK TABLES;

--echo #
--echo # 1.2) ALTER TABLE RENAME in case when several tables are locked.
--echo #
CREATE TABLE t1 (i INT);
LOCK TABLES t1 READ, t2 WRITE;
ALTER TABLE t2 RENAME TO t3;
--echo # Table t1 should be still locked, and t2 should be available as t3
--echo # with correct lock type.
SELECT * FROM t1;
INSERT INTO t3 values (1);
UNLOCK TABLES;

--echo #
--echo # 1.3) ALTER TABLE RENAME in case when same table locked more than once.
--echo #
LOCK TABLES t1 READ, t3 WRITE, t3 AS a WRITE, t3 AS b READ;
ALTER TABLE t3 RENAME TO t4;
--echo # Check that tables are locked under correct aliases and with modes.
SELECT * FROM t4 AS a, t4 AS b;
INSERT INTO t4 VALUES (2);
DELETE a FROM t4 AS a, t4 AS b;
--error ER_TABLE_NOT_LOCKED_FOR_WRITE
DELETE b FROM t4 AS a, t4 AS b;
UNLOCK TABLES;
DROP TABLES t1, t4;

--echo # 1.4) ALTER TABLE RENAME to different schema.
--echo #
CREATE TABLE t1 (i INT);
CREATE DATABASE mysqltest;
LOCK TABLES t1 WRITE;
ALTER TABLE t1 RENAME TO mysqltest.t1;
--echo # Table is available in new schema under LOCK TABLES.
SELECT * FROM mysqltest.t1;

connection con1;
--echo # Access by new name from other connections should be blocked.
SET @@lock_wait_timeout= 1;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM mysqltest.t1;
SET @@lock_wait_timeout= @old_lock_wait_timeout;
--echo # But not to old schema and table name.
--error ER_NO_SUCH_TABLE
SELECT * FROM t1;
--echo # Also IX lock on new schema should be kept.
SET @@lock_wait_timeout= 1;
--error ER_LOCK_WAIT_TIMEOUT
ALTER DATABASE mysqltest CHARACTER SET latin1;
SET @@lock_wait_timeout= @old_lock_wait_timeout;

connection default;
UNLOCK TABLES;
DROP DATABASE mysqltest;

--echo #
--echo # 2) ALTER TABLE INPLACE with RENAME clause.
--echo #
--echo # 2.1) Successful ALTER TABLE INPLACE with RENAME clause.
--echo #
CREATE TABLE t1 (i INT);
LOCK TABLES t1 WRITE;
ALTER TABLE t1 ADD COLUMN j INT, RENAME TO t2, ALGORITHM=INPLACE;
--echo # Table is available under new name under LOCK TABLES.
SELECT * FROM t2;

connection con1;
--echo # Access by new name from other connections should be blocked.
SET @@lock_wait_timeout= 1;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t2;
SET @@lock_wait_timeout= @old_lock_wait_timeout;
--echo # But not for old table name.
--error ER_NO_SUCH_TABLE
SELECT * FROM t1;

connection default;
UNLOCK TABLES;

--echo #
--echo # 2.2) ALTER TABLE INPLACE with RENAME clause in case when several
--echo #      tables are locked.
--echo #
CREATE TABLE t1 (i INT);
LOCK TABLES t1 READ, t2 WRITE;
ALTER TABLE t2 ADD COLUMN k INT, RENAME TO t3, ALGORITHM=INPLACE;
--echo # Table t1 should be still locked, and t2 should be available as t3
--echo # with correct lock type.
SELECT * FROM t1;
INSERT INTO t3 values (1, 2, 3);
UNLOCK TABLES;

--echo #
--echo # 2.3) ALTER TABLE INPLACE with RENAME clause in case when same table
--echo #      locked more than once.
--echo #
LOCK TABLES t1 READ, t3 WRITE, t3 AS a WRITE, t3 AS b READ;
ALTER TABLE t3 ADD COLUMN l INT, RENAME TO t4, ALGORITHM=INPLACE;
--echo # Check that tables are locked under correct aliases and with modes.
SELECT * FROM t4 AS a, t4 AS b;
INSERT INTO t4 VALUES (2, 3, 4, 5);
DELETE a FROM t4 AS a, t4 AS b;
--error ER_TABLE_NOT_LOCKED_FOR_WRITE
DELETE b FROM t4 AS a, t4 AS b;
UNLOCK TABLES;
DROP TABLES t1, t4;

--echo # 2.4) ALTER TABLE INPLACE with RENAME clause to different schema.
--echo #
CREATE TABLE t1 (i INT);
CREATE DATABASE mysqltest;
LOCK TABLES t1 WRITE;
ALTER TABLE t1 ADD COLUMN k INT, RENAME TO mysqltest.t1, ALGORITHM=INPLACE;
--echo # Table is available in new schema under LOCK TABLES.
SELECT * FROM mysqltest.t1;

connection con1;
--echo # Access by new name from other connections should be blocked.
SET @@lock_wait_timeout= 1;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM mysqltest.t1;
SET @@lock_wait_timeout= @old_lock_wait_timeout;
--echo # But not to old schema and table name.
--error ER_NO_SUCH_TABLE
SELECT * FROM t1;
--echo # Also IX lock on new schema should be kept.
SET @@lock_wait_timeout= 1;
--error ER_LOCK_WAIT_TIMEOUT
ALTER DATABASE mysqltest CHARACTER SET latin1;
SET @@lock_wait_timeout= @old_lock_wait_timeout;

connection default;
UNLOCK TABLES;
DROP DATABASE mysqltest;

--echo #
--echo # 3) ALTER TABLE COPY with RENAME clause.
--echo #
--echo # 3.1) Successful ALTER TABLE COPY with RENAME clause.
--echo #
CREATE TABLE t1 (i INT);
LOCK TABLES t1 WRITE;
ALTER TABLE t1 ADD COLUMN j INT, RENAME TO t2, ALGORITHM=COPY;
--echo # Table is available under new name under LOCK TABLES.
SELECT * FROM t2;

connection con1;
--echo # Access by new name from other connections should be blocked.
SET @@lock_wait_timeout= 1;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t2;
SET @@lock_wait_timeout= @old_lock_wait_timeout;
--echo # But not for old table name.
--error ER_NO_SUCH_TABLE
SELECT * FROM t1;

connection default;
UNLOCK TABLES;

--echo #
--echo # 3.2) ALTER TABLE COPY with RENAME clause in case when several
--echo #      tables are locked.
--echo #
CREATE TABLE t1 (i INT);
LOCK TABLES t1 READ, t2 WRITE;
ALTER TABLE t2 ADD COLUMN k INT, RENAME TO t3, ALGORITHM=COPY;
--echo # Table t1 should be still locked, and t2 should be available as t3
--echo # with correct lock type.
SELECT * FROM t1;
INSERT INTO t3 values (1, 2, 3);
UNLOCK TABLES;

--echo #
--echo # 3.3) ALTER TABLE COPY with RENAME clause in case when same table
--echo #      locked more than once.
--echo #
LOCK TABLES t1 READ, t3 WRITE, t3 AS a WRITE, t3 AS b READ;
ALTER TABLE t3 ADD COLUMN l INT, RENAME TO t4, ALGORITHM=COPY;
--echo # Check that tables are locked under correct aliases and with modes.
SELECT * FROM t4 AS a, t4 AS b;
INSERT INTO t4 VALUES (2, 3, 4, 5);
DELETE a FROM t4 AS a, t4 AS b;
--error ER_TABLE_NOT_LOCKED_FOR_WRITE
DELETE b FROM t4 AS a, t4 AS b;
UNLOCK TABLES;
DROP TABLES t1, t4;

--echo # 3.4) ALTER TABLE COPY with RENAME clause to different schema.
--echo #
CREATE TABLE t1 (i INT);
CREATE DATABASE mysqltest;
LOCK TABLES t1 WRITE;
ALTER TABLE t1 ADD COLUMN k INT, RENAME TO mysqltest.t1, ALGORITHM=COPY;
--echo # Table is available in new schema under LOCK TABLES.
SELECT * FROM mysqltest.t1;

connection con1;
--echo # Access by new name from other connections should be blocked.
SET @@lock_wait_timeout= 1;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM mysqltest.t1;
SET @@lock_wait_timeout= @old_lock_wait_timeout;
--echo # But not to old schema and table name.
--error ER_NO_SUCH_TABLE
SELECT * FROM t1;
--echo # Also IX lock on new schema should be kept.
SET @@lock_wait_timeout= 1;
--error ER_LOCK_WAIT_TIMEOUT
ALTER DATABASE mysqltest CHARACTER SET latin1;
SET @@lock_wait_timeout= @old_lock_wait_timeout;

connection default;
UNLOCK TABLES;
DROP DATABASE mysqltest;
