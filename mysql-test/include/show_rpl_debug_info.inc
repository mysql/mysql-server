# ==== Purpose ====
#
# Print status information for replication, typically used to debug
# test failures.
#
# The following is printed on the current connection:
#
#   SELECT NOW(), @@SERVER_ID, @@SERVER_UUID
#   SHOW SLAVE STATUS
#   SHOW MASTER STATUS
#   SHOW PROCESSLIST
#   SHOW BINLOG EVENTS IN <binlog_name>
#
# Where <binlog_name> is the currently active binlog.
#
# Then, the same is printed from all connections configured by
# rpl_init.inc - i.e., on connection server_N, where
# 1 <= N <= $rpl_server_count
#
#
# ==== Usage ====
#
# [--let $rpl_only_current_connection= 1]
# [--let $extra_debug_info= some text]
# [--let $extra_debug_eval= expression parsable by include/eval.inc]
# [--let $extra_debug_table= DB.TABLE]
# --source include/show_rpl_debug_info.inc
#
# Parameters:
#   $rpl_only_current_connection
#     By default, debug info is printed from all connections, starting
#     with the current connection. If this variable is set, debug
#     info is printed only for the current connection.
#
#   $crash_after_show_rpl_debug_info
#     Crash all servers at the end (using include/dbug_crash_all.inc)
#
#   $extra_debug_info
#     This text is printed verbatim to the output. Useful for helper
#     scripts to supply extra diagnostic output.
#
#   $extra_debug_eval
#     Expression in the form expected by include/eval.inc, which will
#     be evaluated and printed to the output.
#
#   $extra_debug_table
#     Fully qualified table name. The script will run SELECT * FROM $table
#     on this table, if given.
#
# ==== Side effects ====
#
# Turns on enable_query_log, enable_result_log, enable_warnings,
# horizontal_results, and enable_abort_on_error.
#
# Prints non-deterministic output to the query log.  This file should
# never be called in a test that does not fail.


--enable_query_log
--enable_result_log
--enable_warnings
--enable_connect_log
--disable_abort_on_error
--horizontal_results


--let $_rpl_old_con= $CURRENT_CONNECTION
--let $_rpl_is_first_server= 1
--let $_rpl_server= $rpl_server_count
if ($_rpl_server == '')
{
  --let $_rpl_server= 0
}
--inc $_rpl_server

--echo DO_NOT_CHECK_IN_THIS_LINE: include/show_rpl_debug_info.inc should only be used for debugging. Never check in a test that calls it on success.

--echo CURRENT_CONNECTION = '$_rpl_old_con'
if($rpl_group_replication)
{
  --echo group_replication_group_name= "$group_replication_group_name"
}

while ($_rpl_server)
{
  if (!$_rpl_is_first_server)
  {
    --connection server_$_rpl_server
  }

  --echo
  --echo ############################## $CURRENT_CONNECTION ##############################
  --echo
  --echo **** SHOW WARNINGS on $CURRENT_CONNECTION ****
  SHOW WARNINGS;
  --echo
  --echo **** SELECT replication-related variables on $CURRENT_CONNECTION ****
  query_vertical
  SELECT NOW(), @@SERVER_ID, @@SERVER_UUID, @@PORT;
  --echo
  --echo **** GTID_* on $CURRENT_CONNECTION ****
  query_vertical
  SELECT @@GLOBAL.GTID_MODE, @@GLOBAL.GTID_EXECUTED, @@GLOBAL.GTID_PURGED, @@GLOBAL.GTID_OWNED, @@SESSION.GTID_OWNED;
  --echo
  --echo **** SHOW SLAVE STATUS on $CURRENT_CONNECTION ****
  query_vertical
   SHOW SLAVE STATUS;
  --echo
  --echo **** replication_connection_status on $CURRENT_CONNECTION ****
  query_vertical
  SELECT * FROM performance_schema.replication_connection_status;
  if($rpl_group_replication)
  {
    --echo
    --echo **** replication_group_members on $CURRENT_CONNECTION ****
    query_vertical
    SELECT * FROM performance_schema.replication_group_members;
    --echo
    --echo **** replication_group_member_stats on $CURRENT_CONNECTION ****
    query_vertical
    SELECT * FROM performance_schema.replication_group_member_stats;
  }
  --echo
  --echo **** replication_applier_status on $CURRENT_CONNECTION ****
  query_vertical
  SELECT * FROM performance_schema.replication_applier_status;
  --echo
  --echo **** replication_applier_status_by_worker on $CURRENT_CONNECTION ****
  query_vertical
  SELECT * FROM performance_schema.replication_applier_status_by_worker;
  --echo
  --echo **** replication_applier_status_by_coordinator on $CURRENT_CONNECTION ****
  query_vertical
  SELECT * FROM performance_schema.replication_applier_status_by_coordinator;
  --echo
  --echo **** SHOW MASTER STATUS on $CURRENT_CONNECTION ****
  query_vertical
  SHOW MASTER STATUS;
  --echo
  --echo **** SHOW SLAVE HOSTS on $CURRENT_CONNECTION ****
  query_vertical
  SHOW SLAVE HOSTS;
  --echo
  --echo **** SHOW PROCESSLIST on $CURRENT_CONNECTION ****
  SHOW PROCESSLIST;
  --echo
  --echo **** SELECT * FROM performance_schema.threads ****
  SELECT * FROM performance_schema.threads;
  --echo
  --echo **** SHOW BINARY LOGS on $CURRENT_CONNECTION ****
  SHOW BINARY LOGS;
  --echo
  --echo **** SHOW BINLOG EVENTS on $CURRENT_CONNECTION ****
  let $binlog_name= query_get_value("SHOW MASTER STATUS", File, 1);
  --echo binlog_name = '$binlog_name'
  eval SHOW BINLOG EVENTS IN '$binlog_name';
  --echo
  --echo **** SHOW RELAYLOG EVENTS on $CURRENT_CONNECTION ****
  let $relaylog_name= query_get_value(SHOW SLAVE STATUS, Relay_Log_File, 1);
  --echo relaylog_name = '$relaylog_name'
  eval SHOW RELAYLOG EVENTS IN '$relaylog_name';
  --echo
  --echo **** slave_relay_info on $CURRENT_CONNECTION ****
  SELECT * FROM mysql.slave_relay_log_info;
  --echo
  --echo **** slave_master_info on $CURRENT_CONNECTION ****
  SELECT * FROM mysql.slave_master_info;
  --echo
  --echo **** mysql.gtid_executed on $CURRENT_CONNECTION ****
  SELECT * FROM mysql.gtid_executed;

  if ($extra_debug_table)
  {
    --echo
    --echo **** $extra_debug_table ****
    eval SHOW CREATE TABLE $extra_debug_table;
    eval SELECT * FROM $extra_debug_table;
  }

  --let $_rpl_is_first_server= 0
  --dec $_rpl_server
  # Don't use same connection twice.
  if (`SELECT 'server_$_rpl_server' = '$_rpl_old_con'`)
  {
    --dec $_rpl_server
    if ($rpl_only_current_connection)
    {
      --let $_rpl_server= 0
    }
  }
}

--echo rpl_topology= $rpl_topology
--echo rand_seed: '$rand_seed' _rand_state: '$_rand_state'
--echo extra debug info if any: '$extra_debug_info'
--echo rpl_topology=$rpl_topology
if ($extra_debug_eval)
{
  --let $eval_expr= $extra_debug_eval
  --source include/eval.inc
  --echo extra evaluated debug expression: '$extra_debug_eval'
  --echo extra evaluated debug info: '$eval_result'
}

if ($crash_after_show_rpl_debug_info)
{
 --source include/dbug_crash_all.inc
}

--connection $_rpl_old_con
--enable_abort_on_error
