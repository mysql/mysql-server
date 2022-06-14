--echo #
--echo # Bug#22710164 xa with innodb allows a table to be dropped while an xa
--echo # transaction has dml
--echo #
--echo # Enable Lock Instrumentation
--enable_connect_log
let $perf_lock_enabled=`select enabled FROM performance_schema.setup_instruments WHERE name='wait/lock/metadata/sql/mdl'`;
UPDATE performance_schema.setup_instruments SET ENABLED=1 WHERE name='wait/lock/metadata/sql/mdl';

--echo
--echo # Test 1: XA Commit after disconnect
connect (con1, localhost, root, ,test);
CREATE TABLE t1 (a INT);
--echo # Query the table t1 in order to load its definition into the data dictionary cache.
--echo # It is required in order to get consistent result for quering from performance_schema.metadata_locks
--echo # when the test case is run both with and without the option --ps-protocol
--sorted_result
SELECT * FROM t1;
XA START 'test';
INSERT INTO t1 VALUES (10);
XA END 'test';
XA PREPARE 'test';

--echo
--echo # Close connection and check lock status form different connection.
disconnect con1;
--source include/wait_until_disconnected.inc
connection default;
--sorted_result
SELECT * FROM t1;
--sorted_result
SELECT object_type, object_schema, object_name, lock_type, lock_duration, lock_status FROM performance_schema.metadata_locks WHERE object_schema = 'test';
SHOW TABLES;
--sorted_result
XA RECOVER;

--echo
--echo # Commit XA and check lock status.
XA COMMIT 'test';
--sorted_result
SELECT object_type, object_schema, object_name, lock_type, lock_duration, lock_status FROM performance_schema.metadata_locks WHERE object_schema = 'test';
--sorted_result
XA RECOVER;
--sorted_result
SELECT * FROM t1;
DROP TABLE t1;

--echo
--echo # Test 2: XA Rollback after disconnect
connect (con1, localhost, root, ,test);
CREATE TABLE t1 (a INT);
--echo # Query the table t1 in order to load its definition into the data dictionary cache.
--echo # It is required in order to get consistent result for quering from performance_schema.metadata_locks
--echo # when the test case is run both with and without the option --ps-protocol
--sorted_result
SELECT * FROM t1;
XA START 'test';
INSERT INTO t1 VALUES (10);
XA END 'test';
XA PREPARE 'test';

--echo
--echo # Close connection and check lock status form different connection.
disconnect con1;
--source include/wait_until_disconnected.inc
connection default;
--sorted_result
SELECT * FROM t1;
--sorted_result
SELECT object_type, object_schema, object_name, lock_type, lock_duration, lock_status FROM performance_schema.metadata_locks WHERE object_schema = 'test';
SHOW TABLES;
--sorted_result
XA RECOVER;

--echo
--echo # Rollback XA and check lock status.
XA ROLLBACK 'test';
--sorted_result
SELECT object_type, object_schema, object_name, lock_type, lock_duration, lock_status FROM performance_schema.metadata_locks WHERE object_schema = 'test';
--sorted_result
XA RECOVER;
--sorted_result
SELECT * FROM t1;
DROP TABLE t1;

--echo
--echo # Test 3: Waiting LOCK requests are not granted during lock transfer to backup
connect (con1, localhost, root, ,test);
CREATE TABLE t1 (a INT);
--echo # Query the table t1 in order to load its definition into the data dictionary cache.
--echo # It is required in order to get consistent result for quering from performance_schema.metadata_locks
--echo # when the test case is run both with and without the option --ps-protocol
--sorted_result
SELECT * FROM t1;
XA START 'test';
INSERT INTO t1 VALUES (10);
XA END 'test';
XA PREPARE 'test';

connect (con2, localhost, root, ,test);
--sorted_result
SELECT * FROM t1;
--send DROP TABLE t1
connection default;
let $wait_condition=SELECT count(*) = 2 from performance_schema.metadata_locks where object_name='t1';
--source include/wait_condition.inc
--echo
--echo # Close connection and check lock status form different connection.
connection con1;
disconnect con1;
--source include/wait_until_disconnected.inc
connection default;
--sorted_result
SELECT object_type, object_schema, object_name, lock_type, lock_duration, lock_status FROM performance_schema.metadata_locks WHERE object_schema = 'test';
SHOW TABLES;
--sorted_result
XA RECOVER;

--echo
--echo # Commit XA and check lock status.
XA COMMIT 'test';
let $wait_condition=SELECT count(*) = 0 from performance_schema.metadata_locks where object_name='t1';
--source include/wait_condition.inc
--sorted_result
SELECT object_type, object_schema, object_name, lock_type, lock_duration, lock_status FROM performance_schema.metadata_locks WHERE object_schema = 'test';
--sorted_result
XA RECOVER;
connection con2;
--reap
disconnect con2;
--source include/wait_until_disconnected.inc
connection default;

--echo
--echo # Test 4: XA Commit after restart
connect (con1, localhost, root, ,test);
CREATE TABLE t1 (a INT);
XA START 'test';
INSERT INTO t1 VALUES (10);
XA END 'test';
XA PREPARE 'test';

--echo
--echo # Restart server and check lock status.
disconnect con1;
--source include/wait_until_disconnected.inc
connection default;
--let $restart_parameters=restart:--log_error_verbosity=1 --performance-schema-instrument='wait/lock/metadata/sql/mdl=1'
--source include/restart_mysqld.inc
--sorted_result
SELECT * FROM t1;
--sorted_result
SELECT object_type, object_schema, object_name, lock_type, lock_duration, lock_status FROM performance_schema.metadata_locks WHERE object_schema = 'test';
SHOW TABLES;
--sorted_result
XA RECOVER;

--echo
--echo # Commit XA and check lock status.
XA COMMIT 'test';
--sorted_result
SELECT object_type, object_schema, object_name, lock_type, lock_duration, lock_status FROM performance_schema.metadata_locks WHERE object_schema = 'test';
--sorted_result
XA RECOVER;
--sorted_result
SELECT * FROM t1;
DROP TABLE t1;

--echo
--echo # Test 5: XA Rollback after restart
connect (con1, localhost, root, ,test);
CREATE TABLE t1 (a INT);
XA START 'test';
INSERT INTO t1 VALUES (10);

XA END 'test';
XA PREPARE 'test';

--echo
--echo # Restart server and check lock status.
disconnect con1;
--source include/wait_until_disconnected.inc
connection default;
--let $restart_parameters=restart:--log_error_verbosity=1 --performance-schema-instrument='wait/lock/metadata/sql/mdl=1'
--source include/restart_mysqld.inc
--sorted_result
SELECT * FROM t1;
--sorted_result
SELECT object_type, object_schema, object_name, lock_type, lock_duration, lock_status FROM performance_schema.metadata_locks WHERE object_schema = 'test';
SHOW TABLES;
--sorted_result
XA RECOVER;

--echo
--echo # Rollback XA and check lock status.
XA ROLLBACK 'test';
--sorted_result
SELECT object_type, object_schema, object_name, lock_type, lock_duration, lock_status FROM performance_schema.metadata_locks WHERE object_schema = 'test';
--sorted_result
XA RECOVER;
--sorted_result
SELECT * FROM t1;
DROP TABLE t1;

--echo
--echo # Test 6: Multiple XA Transaction in Backup
--echo # Insert 3 XA into backup, delete in different order, check lock status
--enable_connect_log
CREATE TABLE t1 (a INT);
CREATE TABLE t2 (a INT);
CREATE TABLE t3 (a INT);
--echo # Query the tables t1, t2, t3 in order to load their definitions into the data dictionary cache.
--echo # It is required in order to get consistent result for quering from performance_schema.metadata_locks
--echo # when the test case is run both with and without the option --ps-protocol
--sorted_result
SELECT * FROM t1;
--sorted_result
SELECT * FROM t2;
--sorted_result
SELECT * FROM t3;
connect (con1, localhost, root, ,test);
XA START 'test1';
INSERT INTO t1 VALUES (10);
XA END 'test1';
XA PREPARE 'test1';
disconnect con1;
--source include/wait_until_disconnected.inc

connect (con1, localhost, root, ,test);
XA START 'test2';
INSERT INTO t2 VALUES (20);
XA END 'test2';
XA PREPARE 'test2';
disconnect con1;
--source include/wait_until_disconnected.inc

connect (con1, localhost, root, ,test);
XA START 'test3';
INSERT  INTO t3 VALUES (30);
XA END 'test3';
XA PREPARE 'test3';
disconnect con1;
--source include/wait_until_disconnected.inc

connection default;
--sorted_result
SELECT object_type, object_schema, object_name, lock_type, lock_duration, lock_status FROM performance_schema.metadata_locks WHERE object_schema = 'test';
SHOW TABLES;
--sorted_result
XA RECOVER;

--echo
--echo # Commit second XA and check lock status.
XA COMMIT 'test2';
--sorted_result
SELECT object_type, object_schema, object_name, lock_type, lock_duration, lock_status FROM performance_schema.metadata_locks WHERE object_schema = 'test';
--sorted_result
XA RECOVER;
DROP TABLE t2;

--echo # Commit First XA
XA COMMIT 'test1';
--sorted_result
SELECT object_type, object_schema, object_name, lock_type, lock_duration, lock_status FROM performance_schema.metadata_locks WHERE object_schema = 'test';
--sorted_result
XA RECOVER;
DROP TABLE t1;

--echo # Commit Last XA
XA COMMIT 'test3';
--sorted_result
SELECT object_type, object_schema, object_name, lock_type, lock_duration, lock_status FROM performance_schema.metadata_locks WHERE object_schema = 'test';
--sorted_result
XA RECOVER;
DROP TABLE t3;

--echo
--echo # Test 7: Multiple XA in recovery
--enable_connect_log
CREATE TABLE t1 (a INT);
CREATE TABLE t2 (a INT);
connect (con1, localhost, root, ,test);
XA START 'test1';
INSERT INTO t1 VALUES (10);
XA END 'test1';
XA PREPARE 'test1';
disconnect con1;
--source include/wait_until_disconnected.inc

connect (con1, localhost, root, ,test);
XA START 'test2';
INSERT INTO t2 VALUES (20);
XA END 'test2';
XA PREPARE 'test2';
disconnect con1;
--source include/wait_until_disconnected.inc

--echo
--echo # Restart server and check lock status.
connection default;
--let $restart_parameters=restart:--log_error_verbosity=1 --performance-schema-instrument='wait/lock/metadata/sql/mdl=1'
--source include/restart_mysqld.inc
--sorted_result
SELECT object_type, object_schema, object_name, lock_type, lock_duration, lock_status FROM performance_schema.metadata_locks WHERE object_schema = 'test';
SHOW TABLES;
--sorted_result
XA RECOVER;

--echo
--echo # Commit First XA
XA COMMIT 'test1';
--sorted_result
SELECT object_type, object_schema, object_name, lock_type, lock_duration, lock_status FROM performance_schema.metadata_locks WHERE object_schema = 'test';
--sorted_result
XA RECOVER;
DROP TABLE t1;

--echo
--echo # Commit second XA
XA COMMIT 'test2';
--sorted_result
SELECT object_type, object_schema, object_name, lock_type, lock_duration, lock_status FROM performance_schema.metadata_locks WHERE object_schema = 'test';
--sorted_result
XA RECOVER;
DROP TABLE t2;

--echo # Cleanup
--eval UPDATE performance_schema.setup_instruments SET ENABLED='$perf_lock_enabled' WHERE name='wait/lock/metadata/sql/mdl'
--disable_connect_log

# Restore default settings
--let $restart_parameters = restart:
--source include/restart_mysqld.inc
