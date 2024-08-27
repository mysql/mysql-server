# WL#16097 Global memory allocation logging limit.

--source include/have_debug.inc
# Save the initial number of concurrent sessions
--source include/count_sessions.inc

CREATE USER 'user1'@localhost;
GRANT USAGE ON *.* TO 'user1'@localhost;

SET GLOBAL global_connection_memory_tracking = ON;

--echo # Check memory status limit cross for init_connect.
SET GLOBAL init_connect = 'show variables';

--echo # Global limit.
SET GLOBAL global_connection_memory_status_limit = 1;

--echo # Connection con1
connect (con1, localhost, user1);
--let $assert_text= 'expected global_connection_memory_status_limit is crossed atleast 1  times now'
--let $assert_cond= "[SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME like \'Count_hit_query_past_global_connection_memory_status_limit\']" > 0
--source include/assert.inc
connection default;
SET GLOBAL global_connection_memory_status_limit = default;
disconnect con1;

--echo # Connection limit.
SET GLOBAL connection_memory_status_limit = 1;
--echo # Connection con1
connect (con1, localhost, user1);
--let $assert_text= 'expected connection_memory_status_limit is crossed atleast 1  times now'
--let $assert_cond= "[SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME like \'Count_hit_query_past_connection_memory_status_limit\']" > 0
--source include/assert.inc
connection default;
SET GLOBAL connection_memory_status_limit = default;
SET GLOBAL init_connect = default;
SET GLOBAL global_connection_memory_tracking = default;
disconnect con1;

DROP USER 'user1'@localhost;

# Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc
