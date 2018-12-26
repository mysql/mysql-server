#
# Bug#18591145 - SOME MONOTONICALLY INCREASING STATUS VARIABLES DECREASES UNEXPECTEDLY
#
CREATE TABLE t1 (id INT PRIMARY KEY AUTO_INCREMENT, name VARCHAR(64), val VARCHAR(1024));
# Insert 1 tuple to increment com_insert status.
INSERT INTO t1(name, val) VALUES ('dummy', 0);
connect  con1, localhost, root,,;
connection con1;
SET DEBUG_SYNC='before_materialize_global_status_array SIGNAL change_user WAIT_FOR continue';
SET DEBUG_SYNC='after_materialize_global_status_array SIGNAL continue_change_user';
INSERT INTO t1(name, val) SELECT * FROM performance_schema.global_status WHERE variable_name='Handler_commit';;
connection default;
SET DEBUG_SYNC='now WAIT_FOR change_user';
SET DEBUG_SYNC='thd_cleanup_start SIGNAL continue WAIT_FOR continue_change_user';
connection con1;
SET DEBUG_SYNC='RESET';
connection default;
INSERT INTO t1(name, val) SELECT * FROM performance_schema.global_status WHERE variable_name='Handler_commit';
# With fix, Handler_commit status should be 2 (select insert during switch user + dictionary).
# With binlog enabled, there is one more Handler_commit call.
SET @binlog_handler_commit= IF(@@global.log_bin, 1, 0);
SELECT (SELECT val FROM t1 WHERE id = 3) - (SELECT val FROM t1 WHERE id = 2) = 1 + @binlog_handler_commit;
(SELECT val FROM t1 WHERE id = 3) - (SELECT val FROM t1 WHERE id = 2) = 1 + @binlog_handler_commit
1
disconnect con1;
DROP TABLE t1;
