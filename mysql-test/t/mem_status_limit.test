# WL#16097 Global memory allocation logging limit.
#--source include/have_debug.inc
# Save the initial number of concurrent sessions
--source include/count_sessions.inc

--echo
--echo # Check access rights for
--echo # connection_memory_status_limit, global_connection_memory_status_limit vars
CREATE USER 'user1'@localhost;
GRANT USAGE ON *.* TO 'user1'@localhost;
GRANT RELOAD ON *.* TO 'user1'@localhost;
GRANT SELECT,DROP ON performance_schema.* TO 'user1'@localhost;

--echo # Connection con1
connect (con1, localhost, user1);

--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET GLOBAL connection_memory_status_limit = 1024 * 1024 * 5;
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET global global_connection_memory_status_limit = 1024 * 1024 * 1024;

--echo # Connection default
--connection default
disconnect con1;

let $query_mem_status_limit= SHOW STATUS LIKE 'count_hit_query_past%';

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
ANALYZE TABLE t1;
SELECT SUM(LENGTH(f1)) FROM t1;
SET GLOBAL global_connection_memory_tracking = ON;
SET GLOBAL group_concat_max_len= 167108864;
SET GLOBAL connection_memory_chunk_size = 1024;
SET GLOBAL connection_memory_status_limit = 1024 * 1024 * 5;
--disable_query_log
call mtr.add_suppression("Connection closed. Connection memory limit.*");
call mtr.add_suppression("Connection closed. Global connection memory limit.*");
--enable_query_log

--echo #
--echo # Test connection_memory_status_limit variable is crossed.
--echo #

--echo
--echo # Testing sql memory key allocation
connect (con1, localhost, user1);
SELECT LENGTH(GROUP_CONCAT(f1 ORDER BY f2)) FROM t1;
disconnect con1;
connection default;
eval $query_mem_status_limit;
--let $assert_text= 'expected connection_memory_status_limit is crossed only once'
--let $assert_cond= "[SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME like \'Count_hit_query_past_connection_memory_status_limit\']" = 1
--source include/assert.inc

--echo
--echo # Testing temptable memory key allocation
connect (con1, localhost, user1);
SET @@tmp_table_size = 64 * 1024 * 1024;
SELECT COUNT(*)
FROM (SELECT SQL_SMALL_RESULT COUNT(*) FROM t1 GROUP By CONCAT(f1,f2)) AS subquery;
disconnect con1;
connection default;
eval $query_mem_status_limit;
--let $assert_text= 'expected connection_memory_status_limit is crossed 2 times now'
--let $assert_cond= "[SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME like \'Count_hit_query_past_connection_memory_status_limit\']" = 2
--source include/assert.inc

SET GLOBAL connection_memory_limit = 1024 * 1024 * 5;

--echo
--echo # Testing sql memory key allocation
connect (con1, localhost, user1);
--replace_regex /Consumed [0-9]+/Consumed SOME/
--error ER_DA_CONN_LIMIT
SELECT LENGTH(GROUP_CONCAT(f1 ORDER BY f2)) FROM t1;
disconnect con1;
connection default;
eval $query_mem_status_limit;
--let $assert_text= 'expected connection_memory_status_limit is crossed 3 times now'
--let $assert_cond= "[SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME like \'Count_hit_query_past_connection_memory_status_limit\']" = 3
--source include/assert.inc

--echo
--echo # Testing temptable memory key allocation
connect (con1, localhost, user1);
SET @@tmp_table_size = 64 * 1024 * 1024;
--replace_regex /Consumed [0-9]+/Consumed SOME/
--error ER_DA_CONN_LIMIT
SELECT COUNT(*)
FROM (SELECT SQL_SMALL_RESULT COUNT(*) FROM t1 GROUP By CONCAT(f1,f2)) AS subquery;
disconnect con1;
connection default;
eval $query_mem_status_limit;
--let $assert_text= 'expected connection_memory_status_limit is crossed 4 times now'
--let $assert_cond= "[SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME like \'Count_hit_query_past_connection_memory_status_limit\']" = 4
--source include/assert.inc

--echo #
--echo # Testing global_connection_memory_status_limit variable is crossed.
--echo #
SET GLOBAL global_connection_memory_status_limit = 1024 * 1024 * 24;
SET GLOBAL connection_memory_status_limit = default;
SET GLOBAL connection_memory_limit = default;
SET GLOBAL global_connection_memory_limit = default;

--echo
--echo # Testing sql memory key allocation
connect (con1, localhost, user1);
SELECT LENGTH(GROUP_CONCAT(f1 ORDER BY f2)) FROM t1;
disconnect con1;
connection default;
eval $query_mem_status_limit;
--let $assert_text= 'expected global_connection_memory_status_limit is crossed 1 times now'
--let $assert_cond= "[SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME like \'Count_hit_query_past_global_connection_memory_status_limit\']" = 1
--source include/assert.inc

--echo
--echo # Testing temptable memory key allocation
connect (con1, localhost, user1);
SET @@tmp_table_size = 64 * 1024 * 1024;
SELECT COUNT(*)
FROM (SELECT SQL_SMALL_RESULT COUNT(*) FROM t1 GROUP By CONCAT(f1,f2)) AS subquery;
disconnect con1;
connection default;
eval $query_mem_status_limit;
--let $assert_text= 'expected global_connection_memory_status_limit is crossed 2 times now'
--let $assert_cond= "[SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME like \'Count_hit_query_past_global_connection_memory_status_limit\']" = 2
--source include/assert.inc

SET GLOBAL global_connection_memory_limit = 1024 * 1024 * 24;

--echo
--echo # Testing sql memory key allocation
connect (con1, localhost, user1);
--replace_regex /Consumed [0-9]+/Consumed SOME/
--error ER_DA_GLOBAL_CONN_LIMIT
SELECT LENGTH(GROUP_CONCAT(f1 ORDER BY f2)) FROM t1;
disconnect con1;
connection default;
eval $query_mem_status_limit;
--let $assert_text= 'expected global_connection_memory_status_limit is crossed 3 times now'
--let $assert_cond= "[SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME like \'Count_hit_query_past_global_connection_memory_status_limit\']" = 3
--source include/assert.inc

--echo
--echo # Testing temptable memory key allocation
connect (con1, localhost, user1);
SET @@tmp_table_size = 64 * 1024 * 1024;
--replace_regex /Consumed [0-9]+/Consumed SOME/
--error ER_DA_GLOBAL_CONN_LIMIT
SELECT COUNT(*)
FROM (SELECT SQL_SMALL_RESULT COUNT(*) FROM t1 GROUP By CONCAT(f1,f2)) AS subquery;
disconnect con1;
connection default;
eval $query_mem_status_limit;
--let $assert_text= 'expected global_connection_memory_status_limit is crossed 4 times now'
--let $assert_cond= "[SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME like \'Count_hit_query_past_global_connection_memory_status_limit\']" = 4
--source include/assert.inc

SET @@tmp_table_size = default;
SET GLOBAL connection_memory_chunk_size = default;
SET GLOBAL global_connection_memory_status_limit = default;
SET GLOBAL connection_memory_status_limit = default;
SET GLOBAL group_concat_max_len = default;
SET GLOBAL global_connection_memory_tracking = default;
SET GLOBAL global_connection_memory_limit = default;
DROP USER 'user1'@localhost;
DROP TABLE t1;

# Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc
