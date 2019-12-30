--source include/force_myisam_default.inc
--source include/have_myisam.inc
--source include/have_debug_sync.inc
--source include/not_threadpool.inc

#
# Test KILL and KILL QUERY statements.
#

connect (con1, localhost, root,,);
connect (con2, localhost, root,,);


#
# Bug#27563: Stored functions and triggers wasn't throwing an error when killed.
#
CREATE TABLE t1 (f1 INT);
delimiter |;
CREATE FUNCTION bug27563() RETURNS INT(11)
DETERMINISTIC
BEGIN
  DECLARE CONTINUE HANDLER FOR SQLSTATE '70100' SET @a:= 'killed';
  DECLARE CONTINUE HANDLER FOR SQLEXCEPTION SET @a:= 'exception';
  SET DEBUG_SYNC= 'now SIGNAL in_sync WAIT_FOR kill';
  RETURN 1;
END|
delimiter ;|
# Test stored functions
# Test INSERT
connection con1;
let $ID= `SELECT @id := CONNECTION_ID()`;
connection con2;
let $ignore= `SELECT @id := $ID`;
connection con1;
send INSERT INTO t1 VALUES (bug27563());
connection con2;
SET DEBUG_SYNC= 'now WAIT_FOR in_sync';
KILL QUERY @id;
connection con1;
--error 1317
reap;
SELECT * FROM t1;
connection default;
SET DEBUG_SYNC = 'RESET';

# Test UPDATE
INSERT INTO t1 VALUES(0);
connection con1;
send UPDATE t1 SET f1= bug27563();
connection con2;
SET DEBUG_SYNC= 'now WAIT_FOR in_sync';
KILL QUERY @id;
connection con1;
--error 1317
reap;
SELECT * FROM t1;
connection default;
SET DEBUG_SYNC = 'RESET';

# Test DELETE
INSERT INTO t1 VALUES(1);
connection con1;
send DELETE FROM t1 WHERE bug27563() IS NULL;
connection con2;
SET DEBUG_SYNC= 'now WAIT_FOR in_sync';
KILL QUERY @id;
connection con1;
--error 1317
reap;
SELECT * FROM t1;
connection default;
SET DEBUG_SYNC = 'RESET';

# Test SELECT
connection con1;
send SELECT * FROM t1 WHERE f1= bug27563();
connection con2;
SET DEBUG_SYNC= 'now WAIT_FOR in_sync';
KILL QUERY @id;
connection con1;
--error 1317
reap;
SELECT * FROM t1;
connection default;
SET DEBUG_SYNC = 'RESET';
DROP FUNCTION bug27563;

# Test TRIGGERS
CREATE TABLE t2 (f2 INT) engine myisam;
delimiter |;
CREATE TRIGGER trg27563 BEFORE INSERT ON t1 FOR EACH ROW
BEGIN
  DECLARE CONTINUE HANDLER FOR SQLSTATE '70100' SET @a:= 'killed';
  DECLARE CONTINUE HANDLER FOR SQLEXCEPTION SET @a:= 'exception';
  INSERT INTO t2 VALUES(0);
  SET DEBUG_SYNC= 'now SIGNAL in_sync WAIT_FOR kill';
  INSERT INTO t2 VALUES(1);
END|
delimiter ;|
connection con1;
send INSERT INTO t1 VALUES(2),(3);
connection con2;
SET DEBUG_SYNC= 'now WAIT_FOR in_sync';
KILL QUERY @id;
connection con1;
--error 1317
reap;
SELECT * FROM t1;
SELECT * FROM t2;
connection default;
SET DEBUG_SYNC = 'RESET';
DROP TABLE t1, t2;

--echo #
--echo # Additional test for WL#3726 "DDL locking for all metadata objects"
--echo # Check that DDL and DML statements waiting for metadata locks can
--echo # be killed. Note that we don't cover all situations here since it
--echo # can be tricky to write test case for some of them (e.g. REPAIR or
--echo # ALTER and other statements under LOCK TABLES).
--echo #


create table t1 (i int primary key) engine myisam;
connect (blocker, localhost, root, , );
connect (dml, localhost, root, , );
connect (ddl, localhost, root, , );

--echo # Test for RENAME TABLE
--echo # Switching to connection 'blocker'
connection blocker;
lock table t1 read;
--echo # Switching to connection 'ddl'
connection ddl;
let $ID= `select connection_id()`;
--send rename table t1 to t2
--echo # Switching to connection 'default'
connection default;
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where state = "Waiting for table metadata lock" and
        info = "rename table t1 to t2";
--source include/wait_condition.inc
--replace_result $ID ID
eval kill query $ID;
--echo # Switching to connection 'ddl'
connection ddl;
--error ER_QUERY_INTERRUPTED
--reap

--echo # Test for DROP TABLE
--send drop table t1
--echo # Switching to connection 'default'
connection default;
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where state = "Waiting for table metadata lock" and
        info = "drop table t1";
--source include/wait_condition.inc
--replace_result $ID ID
eval kill query $ID;
--echo # Switching to connection 'ddl'
connection ddl;
--error ER_QUERY_INTERRUPTED
--reap

--echo # Test for CREATE TRIGGER
--send create trigger t1_bi before insert on t1 for each row set @a:=1
--echo # Switching to connection 'default'
connection default;
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where state = "Waiting for table metadata lock" and
        info = "create trigger t1_bi before insert on t1 for each row set @a:=1";
--source include/wait_condition.inc
--replace_result $ID ID
eval kill query $ID;
--echo # Switching to connection 'ddl'
connection ddl;
--error ER_QUERY_INTERRUPTED
--reap

--echo #
--echo # Tests for various kinds of ALTER TABLE
--echo #
--echo # Full-blown ALTER which should copy table
--send alter table t1 add column j int
--echo # Switching to connection 'default'
connection default;
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where state = "Waiting for table metadata lock" and
        info = "alter table t1 add column j int";
--source include/wait_condition.inc
--replace_result $ID ID
eval kill query $ID;
--echo # Switching to connection 'ddl'
connection ddl;
--error ER_QUERY_INTERRUPTED
--reap

--echo # Two kinds of simple ALTER
--send alter table t1 rename to t2
--echo # Switching to connection 'default'
connection default;
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where state = "Waiting for table metadata lock" and
        info = "alter table t1 rename to t2";
--source include/wait_condition.inc
--replace_result $ID ID
eval kill query $ID;
--echo # Switching to connection 'ddl'
connection ddl;
--error ER_QUERY_INTERRUPTED
--reap
--send alter table t1 disable keys
--echo # Switching to connection 'default'
connection default;
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where state = "Waiting for table metadata lock" and
        info = "alter table t1 disable keys";
--source include/wait_condition.inc
--replace_result $ID ID
eval kill query $ID;
--echo # Switching to connection 'ddl'
connection ddl;
--error ER_QUERY_INTERRUPTED
--reap
--echo # Fast ALTER
--send alter table t1 alter column i set default 100
--echo # Switching to connection 'default'
connection default;
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where state = "Waiting for table metadata lock" and
        info = "alter table t1 alter column i set default 100";
--source include/wait_condition.inc
--replace_result $ID ID
eval kill query $ID;
--echo # Switching to connection 'ddl'
connection ddl;
--error ER_QUERY_INTERRUPTED
--reap
--echo # Special case which is triggered only for MERGE tables.
--echo # Switching to connection 'blocker'
connection blocker;
unlock tables;
create table t2 (i int primary key) engine=merge union=(t1);
lock tables t2 read;
--echo # Switching to connection 'ddl'
connection ddl;
--send alter table t2 alter column i set default 100
--echo # Switching to connection 'default'
connection default;
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where state = "Waiting for table metadata lock" and
        info = "alter table t2 alter column i set default 100";
--source include/wait_condition.inc
--replace_result $ID ID
eval kill query $ID;
--echo # Switching to connection 'ddl'
connection ddl;
--error ER_QUERY_INTERRUPTED
--reap

--echo # Test for DML waiting for meta-data lock
--echo # Switching to connection 'blocker'
connection blocker;
unlock tables;
lock tables t1 read;
--echo # Switching to connection 'ddl'
connection ddl;
# Let us add pending exclusive metadata lock on t2
--send truncate table t1
--echo # Switching to connection 'dml'
connection dml;
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where state = "Waiting for table metadata lock" and
        info = "truncate table t1";
--source include/wait_condition.inc
let $ID2= `select connection_id()`;
--send insert into t1 values (1)
--echo # Switching to connection 'default'
connection default;
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where state = "Waiting for table metadata lock" and
        info = "insert into t1 values (1)";
--source include/wait_condition.inc
--replace_result $ID2 ID2
eval kill query $ID2;
--echo # Switching to connection 'dml'
connection dml;
--error ER_QUERY_INTERRUPTED
--reap
--echo # Switching to connection 'blocker'
connection blocker;
unlock tables;
--echo # Switching to connection 'ddl'
connection ddl;
--reap

--echo # Test for DML waiting for tables to be flushed
--echo # Switching to connection 'blocker'
connection blocker;
lock tables t1 read;
--echo # Switching to connection 'ddl'
connection ddl;
--echo # Let us mark locked table t1 as old
--send flush tables
--echo # Switching to connection 'dml'
connection dml;
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where state = "Waiting for table flush" and
        info = "flush tables";
--source include/wait_condition.inc
--send select * from t1
--echo # Switching to connection 'default'
connection default;
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where state = "Waiting for table flush" and
        info = "select * from t1";
--source include/wait_condition.inc
--replace_result $ID2 ID2
eval kill query $ID2;
--echo # Switching to connection 'dml'
connection dml;
--error ER_QUERY_INTERRUPTED
--reap
--echo # Switching to connection 'blocker'
connection blocker;
unlock tables;
--echo # Switching to connection 'ddl'
connection ddl;
--reap

--echo # Cleanup.
--echo # Switching to connection 'default'
connection default;
drop table t1;
drop table t2;
disconnect con1;
disconnect con2;
