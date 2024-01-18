# ==== Purpose ====
#
# Wait for SQL error and restart replica.
#
# This script is normally used to wait for slave to error out with given SQL
# error code and restart replica. It also asserts last SQL error code and error
# message in performance_schema.replication_applier_status_by_worker.
# And before restarting slave, if gtid to skip is provided it will skip the
# given transaction with injecting empty transaction so slave can be started.
#
# ==== Usage ====
#
# --let $slave_sql_errno= NUMBER
# --let $slave_sql_error= TEXT
# --let $rpl_channel_name= 'NAME'
# [--let $rpl_skip_gtid= UUID:ID]
# --source include/rpl/wait_for_applier_error_and_restart.inc
#
# Parameters:
#
#   $slave_sql_errno
#     The expected SQL error number.
#
#   $slave_sql_error
#     The expected SQL error message.
#
#   $rpl_channel_name
#     Set this to channel you want this script to operate,
#     otherwise it will use the empty channel (channel name '').
#
#   $rpl_skip_gtid
#     The gtid to be skipped with empty transaction.
#
#   $rpl_debug
#     See include/rpl/init.inc
#
--let $include_filename= rpl/wait_for_applier_error_and_restart.inc [errno=$slave_sql_errno]
if ($rpl_channel_name)
{
  --let $include_filename= rpl/wait_for_applier_error_and_restart.inc [errno=$slave_sql_errno FOR CHANNEL $rpl_channel_name]
}
--source include/begin_include_file.inc

if (!$slave_sql_errno)
{
  --die ERROR IN TEST: You must set $slave_sql_errno before sourcing rpl/wait_for_applier_error_and_restart.inc
}

if (!$slave_sql_error)
{
  --die ERROR IN TEST: You must set $slave_sql_error before sourcing rpl/wait_for_applier_error_and_restart.inc
}

if (!$rpl_channel_name)
{
  --let $rpl_channel_name= ''
}

--replace_regex /[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]T[0-9][0-9]:[0-9][0-9]:[0-9][0-9]\.[0-9][0-9][0-9][0-9][0-9][0-9][-+Z][0-9:]* *[0-9]* *\[/DATE_TIME [/
--let $show_slave_sql_error= 0
--let $slave_sql_error_replace= / at [0-9]*/ at XXX/
--source include/rpl/wait_for_applier_error.inc

# Wait for slave sql channel last error number
--let $wait_condition= SELECT COUNT(*)=1 FROM performance_schema.replication_applier_status_by_worker WHERE channel_name=$rpl_channel_name AND LAST_ERROR_NUMBER=$slave_sql_errno
--source include/wait_condition.inc

# Wait for slave sql channel last error message
--let $wait_condition= SELECT COUNT(*)=1 LAST_ERROR_MESSAGE_COUNT FROM performance_schema.replication_applier_status_by_worker WHERE channel_name=$rpl_channel_name AND LAST_ERROR_MESSAGE REGEXP '.*$slave_sql_error.*'
--source include/wait_condition.inc

# Wait for slave sql channel state to be OFF
--let $wait_condition= SELECT COUNT(*)=1 FROM performance_schema.replication_applier_status WHERE channel_name=$rpl_channel_name AND SERVICE_STATE="OFF"
--source include/wait_condition.inc

--source include/rpl/stop_replica.inc

--disable_query_log
--eval RESET REPLICA FOR CHANNEL $rpl_channel_name

# skip $rpl_skip_gtid transaction
if ($rpl_skip_gtid)
{
  --eval SET GTID_NEXT= $rpl_skip_gtid
  BEGIN; COMMIT;
  SET GTID_NEXT= AUTOMATIC;
}
--enable_query_log

--source include/rpl/start_replica.inc

--let $include_filename= rpl/wait_for_applier_error_and_restart.inc [errno=$slave_sql_errno]
--source include/end_include_file.inc
