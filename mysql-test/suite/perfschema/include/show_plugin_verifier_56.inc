--disable_warnings

--echo
SHOW GLOBAL STATUS LIKE "example_%";
--echo
SHOW SESSION STATUS LIKE "example_%";
--echo
SHOW GLOBAL VARIABLES LIKE "example_%";
--echo
SHOW SESSION VARIABLES LIKE "example_%";
--echo
SELECT variable_name, variable_value FROM information_schema.global_status WHERE variable_name LIKE "example_%";
--echo
SELECT variable_name, variable_value FROM information_schema.session_status WHERE variable_name LIKE "example_%";
--echo
SELECT variable_name, variable_value FROM information_schema.global_variables WHERE variable_name LIKE "example_%";
--echo
SELECT variable_name, variable_value FROM information_schema.session_variables WHERE variable_name LIKE "example_%";

#
# Force sync of local and global system variables.
#
--disable_result_log
--disable_query_log
SELECT variable_name, variable_value FROM performance_schema.variables_by_thread WHERE variable_name LIKE "example_%";
--enable_query_log
--enable_result_log

--enable_warnings
