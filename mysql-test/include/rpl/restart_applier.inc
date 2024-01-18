# ==== Purpose ====
#
# Provide a earier way to restart SQL thread when you want to stop sql thread
# and then start it immediately.
#
# Sources rpl/stop_applier.inc to stop SQL thread on the current connection.
# Then issues START REPLICA SQL_THREAD and then waits until
# the SQL threads have started, or until a timeout is reached.
#
# Please use this instead of 'STOP|START REPLICA SQL_THREAD', to reduce the risk of
# test case bugs.
#
#
# ==== Usage ====
#
# [--let $slave_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/rpl/restart_applier.inc
#
# Parameters:
#   $slave_timeout
#     See include/rpl/wait_for_replica_status.inc
#
#   $rpl_debug
#     See include/rpl/init.inc


--let $include_filename= rpl/restart_applier.inc
--source include/begin_include_file.inc


if (!$rpl_debug)
{
  --disable_query_log
}

source include/rpl/stop_applier.inc;
START REPLICA SQL_THREAD;
source include/rpl/wait_for_applier_to_start.inc;


--let $include_filename= rpl/restart_applier.inc
--source include/end_include_file.inc
