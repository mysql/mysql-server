# ==== Purpose ====
#
# Provide a earier way to restart SQL thread when you want to stop sql thread
# and then start it immediately.
#
# Sources stop_slave_sql.inc to stop SQL thread on the current connection.
# Then issues START SLAVE SQL_THREAD and then waits until
# the SQL threads have started, or until a timeout is reached.
#
# Please use this instead of 'STOP|START SLAVE SQL_THREAD', to reduce the risk of
# test case bugs.
#
#
# ==== Usage ====
#
# [--let $slave_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/restart_slave_sql.inc
#
# Parameters:
#   $slave_timeout
#     See include/wait_for_slave_param.inc
#
#   $rpl_debug
#     See include/rpl_init.inc


--let $include_filename= restart_slave.inc
--source include/begin_include_file.inc


if (!$rpl_debug)
{
  --disable_query_log
}

source include/stop_slave_sql.inc;
START SLAVE SQL_THREAD;
source include/wait_for_slave_sql_to_start.inc;


--let $include_filename= restart_slave.inc
--source include/end_include_file.inc
