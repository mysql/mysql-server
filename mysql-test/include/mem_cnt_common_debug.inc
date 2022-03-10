# WL#13458 Global and session memory allocation limits.

# Save the initial number of concurrent sessions
--source include/count_sessions.inc

CREATE USER 'user1'@localhost;
GRANT USAGE ON *.* TO 'user1'@localhost;

let $query_check_err_log=
  SELECT PRIO, SUBSYSTEM, DATA FROM performance_schema.error_log
  ORDER BY LOGGED DESC LIMIT 1;

SET GLOBAL global_connection_memory_tracking = ON;


--echo # Check OOM for init_connect.
SET GLOBAL init_connect = 'show variables';

--echo # Global limit.
SET GLOBAL global_connection_memory_limit = 1;

--echo # Connection con1
connect (con1, localhost, user1);
--replace_regex /connection .* to/connection to/
--error ER_NEW_ABORTING_CONNECTION
SELECT 1;
connection default;
--replace_regex /connection .* to/connection to/ /MY-[0-9]+/MY-NNNNNN/
eval $query_check_err_log;
SET GLOBAL global_connection_memory_limit = default;
disconnect con1;

--echo # Connection limit.
SET GLOBAL connection_memory_limit = 1;
--echo # Connection con1
connect (con1, localhost, user1);
--replace_regex /connection .* to/connection to/
--error ER_NEW_ABORTING_CONNECTION
SELECT 1;
connection default;
--replace_regex /connection .* to/connection to/ /MY-[0-9]+/MY-NNNNNN/
eval $query_check_err_log;
SET GLOBAL connection_memory_limit = default;
disconnect con1;

SET GLOBAL init_connect = default;

--echo # Check OOM before idle stage at connection stage.

--echo # Global limit.
SET GLOBAL global_connection_memory_limit = 1;

--echo # Connection con1
connect (con1, localhost, user1);
--replace_regex /Consumed [0-9]+/Consumed SOME/
--error ER_DA_GLOBAL_CONN_LIMIT
SELECT 1;
connection default;
--replace_regex /Consumed [0-9]+/Consumed SOME/
eval $query_check_err_log;
SET GLOBAL global_connection_memory_limit = default;
disconnect con1;

--echo # Connection limit.
SET GLOBAL connection_memory_limit = 1;
--echo # Connection con1
connect (con1, localhost, user1);
--replace_regex /Consumed [0-9]+/Consumed SOME/
--error ER_DA_CONN_LIMIT
SELECT 1;
connection default;
--replace_regex /Consumed [0-9]+/Consumed SOME/
eval $query_check_err_log;
SET GLOBAL connection_memory_limit = default;
disconnect con1;

--echo # Check OOM before idle stage after succesful statement execution.

let $query_check_err_log=
  SELECT PRIO, SUBSYSTEM, DATA FROM performance_schema.error_log
  WHERE DATA LIKE '%memory limit 2 bytes exceeded%'
  ORDER BY LOGGED DESC LIMIT 1;

GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO 'user1'@localhost;

--echo # Global limit.

connect (con3, localhost, user1);
SET SESSION DEBUG="+d,mem_cnt_no_error_on_exec_global";

connection default;
SET GLOBAL global_connection_memory_limit = 2;
connection con3;
--echo # Both statements should be executed without errors.
--disable_result_log
SHOW VARIABLES;
SHOW VARIABLES;
--enable_result_log

connection default;
disconnect con3;
SET GLOBAL global_connection_memory_limit = default;

--echo # Session limit.

SET GLOBAL DEBUG="+d,mem_cnt_no_error_on_exec_session";
SET GLOBAL connection_memory_limit = 10000;
connect (con3, localhost, user1);
--echo # Both statements should be executed without errors.
--disable_result_log
SHOW VARIABLES;
SHOW VARIABLES;
--enable_result_log

connection default;
disconnect con3;
SET GLOBAL DEBUG = default;

SET GLOBAL global_connection_memory_limit = default;
SET GLOBAL connection_memory_limit = default;
SET GLOBAL global_connection_memory_tracking = default;
DROP USER 'user1'@localhost;

# Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc
