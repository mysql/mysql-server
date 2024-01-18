#
--source include/have_debug_sync.inc

# Skipping the test when binlog_format=STATEMENT due to unsafe statements:
# unsafe operation on auto-increment column, and performance_schema table.
--source include/rpl/deprecated/not_binlog_format_statement.inc

# Save the initial number of concurrent sessions.
--source include/count_sessions.inc

--echo #
--echo # Bug#18591145 - SOME MONOTONICALLY INCREASING STATUS VARIABLES DECREASES UNEXPECTEDLY
--echo #

--enable_connect_log

CREATE TABLE t1 (id INT PRIMARY KEY AUTO_INCREMENT, name VARCHAR(64), val VARCHAR(1024));

--echo # Insert 1 tuple to increment com_insert status.
INSERT INTO t1(name, val) VALUES ('dummy', 0);

connect (con1, localhost, root,,);
CONNECTION con1;
SET DEBUG_SYNC='before_materialize_global_status_array SIGNAL change_user WAIT_FOR continue';
SET DEBUG_SYNC='after_materialize_global_status_array SIGNAL continue_change_user';
--SEND INSERT INTO t1(name, val) SELECT * FROM performance_schema.global_status WHERE variable_name='Handler_commit';

CONNECTION default;
SET DEBUG_SYNC='now WAIT_FOR change_user';
SET DEBUG_SYNC='thd_cleanup_start SIGNAL continue WAIT_FOR continue_change_user';
--change_user root,,test

CONNECTION con1;
--REAP

SET DEBUG_SYNC='RESET';
CONNECTION default;
INSERT INTO t1(name, val) SELECT * FROM performance_schema.global_status WHERE variable_name='Handler_commit';

--echo # With fix, Handler_commit status should be 2 (select insert during switch user + dictionary).
--echo # With binlog enabled, there is one more Handler_commit call.
SET @binlog_handler_commit= IF(@@global.log_bin, 1, 0);
SELECT (SELECT val FROM t1 WHERE id = 3) - (SELECT val FROM t1 WHERE id = 2) = 1 + @binlog_handler_commit;

# Cleanup
DISCONNECT con1;
DROP TABLE t1;

--disable_connect_log

# Wait till we reached the initial number of concurrent sessions
--source include/wait_until_count_sessions.inc
