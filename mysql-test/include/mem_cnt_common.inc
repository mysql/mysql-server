# WL#13458 Global and session memory allocation limits.

# Save the initial number of concurrent sessions
--source include/count_sessions.inc

--echo
--echo # Check variables for min/default values.
SET SESSION connection_memory_limit = 1024 * 1024;
SET SESSION connection_memory_limit = 1024 * 1024 * 2;
SET GLOBAL global_connection_memory_limit = 1024 * 1024;
SET GLOBAL global_connection_memory_limit = 1024 * 1024 * 16;
SET SESSION connection_memory_chunk_size = 1024 * 1024;
SET SESSION connection_memory_chunk_size = 1024 * 1024 * 16;
SHOW VARIABLES like '%connection_memory%';
SET SESSION connection_memory_limit = default;
SET SESSION connection_memory_chunk_size = default;
SET GLOBAL global_connection_memory_limit = default;

--echo
--echo # Check access rights for
--echo # connection_memory_limit, global_connection_memory_limit vars
CREATE USER 'user1'@localhost;
GRANT USAGE ON *.* TO 'user1'@localhost;
GRANT RELOAD ON *.* TO 'user1'@localhost;
GRANT SELECT,DROP ON performance_schema.* TO 'user1'@localhost;

--echo # Connection con1
connect (con1, localhost, user1);

--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET SESSION connection_memory_limit = 1024 * 1024 * 2;
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET global global_connection_memory_limit = 1024 * 1024 * 16;
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET SESSION connection_memory_chunk_size = 1024 * 1024;
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET SESSION global_connection_memory_tracking = false;

--echo # Connection default
--connection default
disconnect con1;

let $query_truncate_pfs=
  TRUNCATE TABLE performance_schema.events_waits_summary_by_account_by_event_name;
let $query_num_locks=
  SELECT COUNT_STAR > 0 FROM performance_schema.events_waits_summary_by_account_by_event_name
  WHERE USER = 'user1' AND EVENT_NAME LIKE 'wait/synch/mutex/sql/LOCK_global_conn_mem_limit';

let $query_check_err_log=
  SELECT PRIO, SUBSYSTEM, DATA FROM performance_schema.error_log
  ORDER BY LOGGED DESC LIMIT 1;

CREATE TABLE t1 (f1 LONGTEXT , f2  INTEGER);
INSERT INTO t1 VALUES 
(REPEAT('a', 1024), 0), (REPEAT('b', 1024), 1),
(REPEAT('c', 1024), 2), (REPEAT('d', 1024), 3),
(REPEAT('e', 1024), 4), (REPEAT('f', 1024), 5);
INSERT INTO t1 SELECT f1, f2 + 6 FROM t1;
INSERT INTO t1 SELECT f1, f2 + 12 FROM t1;
INSERT INTO t1 SELECT f1, f2 + 24 FROM t1;
INSERT INTO t1 SELECT f1, f2 + 48 FROM t1;
INSERT INTO t1 SELECT f1, f2 + 96 FROM t1;
INSERT INTO t1 SELECT f1, f2 + 192 FROM t1;
INSERT INTO t1 SELECT f1, f2 + 384 FROM t1;
INSERT INTO t1 SELECT f1, f2 + 768 FROM t1;
INSERT INTO t1 SELECT f1, f2 + 1536 FROM t1;
INSERT INTO t1 SELECT f1, f2 + 3072 FROM t1;
INSERT INTO t1 SELECT f1, f2 + 6144 FROM t1;
INSERT INTO t1 SELECT f1, f2 + 12288 FROM t1;
SELECT SUM(LENGTH(f1)) FROM t1;

SET GLOBAL connection_memory_limit = 1024 * 1024 * 2;
SET GLOBAL group_concat_max_len= 167108864;
SET GLOBAL connection_memory_chunk_size = 1024 * 1024;

--echo #
--echo # Testing OOM for connection_memory_limit variable.
--echo #

--echo
--echo # Testing sql memory key allocation
eval $query_truncate_pfs;
connect (con1, localhost, user1);
--replace_regex /Consumed [0-9]+/Consumed SOME/
--error ER_DA_CONN_LIMIT
SELECT LENGTH(GROUP_CONCAT(f1 ORDER BY f2)) FROM t1;
disconnect con1;
connection default;
eval $query_num_locks;
--replace_regex /Consumed [0-9]+/Consumed SOME/
eval $query_check_err_log;

--echo
--echo # Testing temptable memory key allocation
eval $query_truncate_pfs;
connect (con1, localhost, user1);
SET @@tmp_table_size = 32 * 1024 * 1024;
--replace_regex /Consumed [0-9]+/Consumed SOME/
--error ER_DA_CONN_LIMIT
SELECT COUNT(*) FROM t1 GROUP By CONCAT(f1,f2);
disconnect con1;
connection default;
eval $query_num_locks;
--replace_regex /Consumed [0-9]+/Consumed SOME/
eval $query_check_err_log;

--echo #
--echo # Testing OOM for global_connection_memory_limit variable.
--echo #
SET GLOBAL global_connection_memory_limit = 1024 * 1024 * 16;
SET GLOBAL connection_memory_limit = default;

--echo
--echo # Testing sql memory key allocation
eval $query_truncate_pfs;
connect (con1, localhost, user1);
--replace_regex /Consumed [0-9]+/Consumed SOME/
--error ER_DA_GLOBAL_CONN_LIMIT
SELECT LENGTH(GROUP_CONCAT(f1 ORDER BY f2)) FROM t1;
disconnect con1;
connection default;
eval $query_num_locks;
--replace_regex /Consumed [0-9]+/Consumed SOME/
eval $query_check_err_log;

--echo
--echo # Testing temptable memory key allocation
eval $query_truncate_pfs;
connect (con1, localhost, user1);
SET @@tmp_table_size = 32 * 1024 * 1024;
--replace_regex /Consumed [0-9]+/Consumed SOME/
--error ER_DA_GLOBAL_CONN_LIMIT
SELECT COUNT(*) FROM t1 GROUP By CONCAT(f1,f2);
disconnect con1;
connection default;
eval $query_num_locks;
--replace_regex /Consumed [0-9]+/Consumed SOME/
eval $query_check_err_log;

SET @@tmp_table_size = default;
SET GLOBAL connection_memory_chunk_size = default;
SET GLOBAL global_connection_memory_limit = default;
SET GLOBAL group_concat_max_len= default;
DROP USER 'user1'@localhost;
DROP TABLE t1;

# Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc
