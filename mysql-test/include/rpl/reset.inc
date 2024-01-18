# ==== Purpose ====
#
# Reset all replication servers to a clean state:
#
#  - sync all slaves,
#  - stop all slaves (STOP REPLICA),
#  - remove all binlogs and relay logs (RESET BINARY LOGS AND GTIDS and RESET REPLICA),
#  - start all slaves again (START REPLICA).
#
# It does not execute CHANGE REPLICATION SOURCE, so the replication topology is
# kept intact.
#
#
# ==== Usage ====
#
# [--let $rpl_only_running_threads= 1]
# [--let $rpl_debug= 1]
# [--let $slave_timeout= NUMBER]
# --source include/rpl/reset.inc
#
# Parameters:
#   $rpl_only_running_threads
#     If one or both of the IO and SQL threads is stopped, sync and
#     stop only the threads that are running.  See
#     include/rpl/sync.inc and include/rpl/stop_replica.inc for details.
#
#   $rpl_skip_sync
#     By default, all slaves are synced using rpl/sync.inc. Set this
#     option to 1 to disable this behavior (note that you must
#     manually sync all servers in this case). Normally you want to
#     sync, but you need to disable sync if you use multi-source.
#
#   $rpl_debug
#     See include/rpl/init.inc
#
#   $rpl_no_start_slave
#     Don't execute START REPLICA.
#
#   $slave_timeout
#     Set the timeout when waiting for replica threads to stop and
#     start, respectively.  See include/rpl/wait_for_replica_status.inc
#
# Note:
#   This script will fail if Last_SQL_Error or Last_IO_Error is
#   nonempty.  If you expect an error in the SQL thread, you should
#   normally do this before you source include/rpl/reset.inc:
#
#     --source include/rpl/wait_for_applier_error.inc
#     --source include/rpl/stop_receiver.inc
#     RESET REPLICA;

--let $include_filename= rpl/reset.inc
--source include/begin_include_file.inc

if (!$rpl_debug)
{
  --disable_query_log
}

# Sync
if (!$rpl_skip_sync)
{
  --source include/rpl/sync.inc
}

# STOP REPLICA
--source include/rpl/stop_all_replicas.inc

# RESET REPLICA
--let $rpl_source_file= include/rpl/impl/reset_replica_helper.inc
--source include/rpl/for_each_connection.inc

# RESET BINARY LOGS AND GTIDS
--let $rpl_sql= RESET BINARY LOGS AND GTIDS
--source include/rpl/for_each_server_stmt.inc

# START REPLICA
if (!$rpl_no_start_slave)
{
  --source include/rpl/start_all_replicas.inc
}


--let $include_filename= rpl/reset.inc
--source include/end_include_file.inc
