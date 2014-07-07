# ==== Purpose ====
#
# Waits until the SQL thread of the current connection has stopped
# gracefully.
#
# Note: this script will fail if the SQL thread stops with an error.
# If you expect an error in the SQL thread, use
# include/wait_for_slave_io_error.inc instead.
#
# This script also fails if a timeout is reached (default 300
# seconds).
#
#
# ==== Usage ====
#
# [--let $slave_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/wait_for_slave_sql_to_stop.inc
#
# Parameters:
#   $slave_timeout
#     See include/wait_for_slave_param.inc
#
#   $rpl_debug
#     See include/rpl_init.inc


--let $include_filename= wait_for_slave_sql_to_stop.inc
--source include/begin_include_file.inc


--let $slave_param= Slave_SQL_Running
--let $slave_param_value= No
--let $slave_param_comparison= =
--let $slave_error_param= Last_SQL_Errno
--source include/wait_for_slave_param.inc
--let $slave_error_param= 


--let $include_filename= wait_for_slave_sql_to_stop.inc
--source include/end_include_file.inc
