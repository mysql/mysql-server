# ==== Purpose ====
#
# Print status information for replication, typically used to debug
# test failures.
#
# First, various debug-related queries are executed on every server
# configured in rpl_init.inc or master-slave.inc.
#
# Then, global/server-independent debug information is printed.
#
# This script tries to put the most commonly useful information in a
# summarized form last, since mtr echoes the last 30 lines of the test
# log to stdout. The full log remains in the var dir, but sometimes that
# is hard to access whereas mtr's stdout is usually always accessible.
#
# ==== Usage ====
#
# [--let $rpl_only_current_connection = 1]
# [--let $crash_after_show_rpl_debug_info = 1]
# [--let $extra_debug_info = some text]
# [--let $extra_debug_eval = expression parsable by include/eval.inc]
# [--let $extra_debug_table = DB.TABLE]
# [--let $extra_debug_sql = STATEMENT]
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
#   $extra_debug_sql
#     If this is set to a statement, the statement will be executed on
#     each connection.  This can also be a list of statements,
#     delimited by semicolons.  Therefore, a good way to use this is to
#     append to it, like:
#     --let $extra_debug_sql = $extra_debug_sql ; STATEMENT
#     In case some test utility has set the variable, appending to it
#     this way does not remove existing debug info.
#
# ==== Side effects ====
#
# Turns on enable_query_log, enable_result_log, enable_warnings,
# horizontal_results, and disable_abort_on_error.
#
# Prints non-deterministic output to the query log.  This file should
# never be called in a test that does not fail.


--enable_query_log
--enable_result_log
--enable_warnings
--enable_connect_log
--disable_abort_on_error
--horizontal_results


--let $_rpl_old_con = $CURRENT_CONNECTION

--echo DO_NOT_CHECK_IN_THIS_LINE: include/show_rpl_debug_info.inc should only be used for debugging. Never check in a test that calls it on success.

--echo CURRENT_CONNECTION = '$_rpl_old_con'
if($rpl_group_replication)
{
  --echo group_replication_group_name = "$group_replication_group_name"
}

# Before starting to dump global info on each server, dump session
# info for current connection.
--echo **** SHOW WARNINGS on $CURRENT_CONNECTION ****
SHOW WARNINGS;
SELECT @@SERVER_ID;

--let $_rpl_errors =
--let $_log_errors =

# Handle the case that replication has not been configured.
--let $change_connection = 1
if ($rpl_server_count == '') {
  --let $rpl_server_count = 1
  --let $change_connection = 0
}

--let $_rpl_server = 1
while ($_rpl_server <= $rpl_server_count) {
  if ($change_connection) {
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
  --echo **** SELECT performance_schema.error_log on $CURRENT_CONNECTION ****
  SELECT * FROM performance_schema.error_log;

  --echo
  --echo **** GTID_* on $CURRENT_CONNECTION ****
  query_vertical
  SELECT @@GLOBAL.GTID_MODE, @@GLOBAL.GTID_EXECUTED, @@GLOBAL.GTID_PURGED, @@GLOBAL.GTID_OWNED, @@SESSION.GTID_OWNED;

  --echo
  --echo **** SHOW REPLICA STATUS on $CURRENT_CONNECTION ****
  query_vertical
  SHOW REPLICA STATUS;

  --echo
  --echo **** replication_connection_status on $CURRENT_CONNECTION ****
  query_vertical
  SELECT * FROM performance_schema.replication_connection_status;

  if ($rpl_group_replication) {
    --echo
    --echo **** replication_group_members on $CURRENT_CONNECTION ****
    query_vertical
    SELECT * FROM performance_schema.replication_group_members;

    --echo
    --echo **** replication_group_member_stats on $CURRENT_CONNECTION ****
    query_vertical
    SELECT * FROM performance_schema.replication_group_member_stats;

    --echo
    --echo **** replication_group_configuration_version on $CURRENT_CONNECTION ****
    query_vertical
    SELECT * FROM performance_schema.replication_group_configuration_version;

    --echo
    --echo **** replication_group_member_actions on $CURRENT_CONNECTION ****
    query_vertical
    SELECT * FROM performance_schema.replication_group_member_actions;

    --echo
    --echo **** replication_group_communication_information on $CURRENT_CONNECTION ****
    query_vertical
    SELECT * FROM performance_schema.replication_group_communication_information;

    --echo
    --echo **** group replication stages on $CURRENT_CONNECTION ****
    query_vertical
    SELECT * FROM performance_schema.events_stages_current WHERE event_name LIKE "%stage/group_rpl%";
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
  --echo **** replication_applier_configuration on $CURRENT_CONNECTION ****
  query_vertical
  SELECT * FROM performance_schema.replication_applier_configuration;

  --echo
  --echo **** replication_asynchronous_connection_failover on $CURRENT_CONNECTION ****
  query_vertical
  SELECT * FROM performance_schema.replication_asynchronous_connection_failover;

  --echo
  --echo **** replication_asynchronous_connection_failover_managed on $CURRENT_CONNECTION ****
  query_vertical
  SELECT * FROM performance_schema.replication_asynchronous_connection_failover_managed;

  --echo
  --echo **** SHOW MASTER STATUS on $CURRENT_CONNECTION ****
  query_vertical
  SHOW MASTER STATUS;

  --echo
  --echo **** SHOW REPLICAS on $CURRENT_CONNECTION ****
  query_vertical
  SHOW REPLICAS;

  --echo
  --echo **** SHOW PROCESSLIST on $CURRENT_CONNECTION ****
  SHOW PROCESSLIST;

  --echo
  --echo **** SELECT * FROM performance_schema.threads on $CURRENT_CONNECTION ****
  SELECT * FROM performance_schema.threads;

  --echo
  --echo **** SHOW BINARY LOGS on $CURRENT_CONNECTION ****
  SHOW BINARY LOGS;

  --echo
  --echo **** SHOW BINLOG EVENTS on $CURRENT_CONNECTION ****
  let $binlog_name = query_get_value("SHOW MASTER STATUS", File, 1);
  --echo binlog_name = '$binlog_name'
  eval SHOW BINLOG EVENTS IN '$binlog_name';

  --echo
  --echo **** SHOW RELAYLOG EVENTS on $CURRENT_CONNECTION ****
  let $relaylog_name = query_get_value(SHOW REPLICA STATUS, Relay_Log_File, 1);
  --echo applier relay_log_name = '$relaylog_name'
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

  if ($extra_debug_table) {
    --echo
    --echo **** extra_debug_table on $CURRENT_CONNECTION ****
    --echo # $extra_debug_table
    eval SHOW CREATE TABLE $extra_debug_table;
    eval SELECT * FROM $extra_debug_table;
  }

  if ($extra_debug_sql != '') {
    --echo
    --echo **** extra_debug_sql on $CURRENT_CONNECTION ****
    --echo # $extra_debug_sql
    --eval $extra_debug_sql
  }

  # Compute messages to print at the end.  This should be a short
  # summary of the most critical information.  We print it at the end
  # because mtr echoes the 30 last lines of the log to stdout, meaning
  # such information is much quicker to find, and sometimes the full
  # log is lost (when the vardir has been removed).

  # *rpl_error*: For each channel on each server, this contains the
  # receiver error, the coordinator error, and the applier errors in
  # each worker.
  --let $_worker_errors = `SELECT GROUP_CONCAT(CONCAT('CHANNEL:<', CHANNEL_NAME, '> WORKER:', WORKER_ID, ' ERROR:<', LAST_ERROR_MESSAGE, '>') ORDER BY CHANNEL_NAME, WORKER_ID) FROM performance_schema.replication_applier_status_by_worker WHERE LAST_ERROR_MESSAGE != '' GROUP BY ''`
  --let $_receiver_errors = `SELECT GROUP_CONCAT(CONCAT('CHANNEL:<', CHANNEL_NAME, '> ERROR:<', LAST_ERROR_MESSAGE, '>')) FROM performance_schema.replication_connection_status WHERE LAST_ERROR_MESSAGE != '' GROUP BY ''`
  --let $_coordinator_errors = `SELECT GROUP_CONCAT(CONCAT('CHANNEL:<', CHANNEL_NAME, '> ERROR:<', LAST_ERROR_MESSAGE, '>')) FROM performance_schema.replication_applier_status_by_coordinator WHERE LAST_ERROR_MESSAGE != '' GROUP BY ''`
  --let $_errors =
  if ($_worker_errors) {
    --let $_errors = WORKERS:($_worker_errors)
  }
  if ($_receiver_errors) {
    --let $_errors = $_errors RECEIVERS:($_receiver_errors)
  }
  if ($_coordinator_errors) {
    --let $_errors = $_errors COORDINATORS:($_coordinator_errors)
  }
  if ($_errors) {
    if ($_rpl_errors) {
      # If $_rpl_errors was set in a previous iteration, separate with ';'.
      --let $_rpl_errors = $_rpl_errors;
    }
    --let $_rpl_errors = $_rpl_errors SERVER_$_rpl_server:($_errors)
  }

  # *error_log*: For each server, the this contains the last error in
  # performance_schema.error_log.
  --let $_last_log_error = `SELECT CONCAT('Thread: ', THREAD_ID, ' Error: ', ERROR_CODE, ': ', DATA) FROM performance_schema.error_log WHERE PRIO = 'Error' ORDER BY LOGGED DESC LIMIT 1`
  if (!$_last_log_error) {
    --let $_last_log_error = none
  }
  if ($_log_errors) {
    # If $_log_errors was set in a previous iteration, separate with ';'.
    --let $_log_errors = $_log_errors;
  }
  --let $_log_errors = $_log_errors SERVER_$_rpl_server:<$_last_log_error>

  --inc $_rpl_server
}

--echo
--echo rpl_topology=$rpl_topology

if ($_rand_state) {
  --echo rand_seed: '$rand_seed' _rand_state: '$_rand_state'
}

--echo extra debug info if any: '$extra_debug_info'

if ($extra_debug_eval) {
  --let $eval_expr = $extra_debug_eval
  --source include/eval.inc
  --echo extra evaluated debug expression: '$extra_debug_eval'
  --echo extra evaluated debug info: '$eval_result'
}

# Print tests executing prior to the current one.
--connection default
--let ERROR_LOG_FILENAME = `SELECT IF(@@global.log_error = 'stderr', '$MYSQLTEST_VARDIR/log/mysqld.1.err', @@global.log_error)`
perl;
  my $ef = $ENV{'ERROR_LOG_FILENAME'};
  open ERROR_FILE, "< $ef" or die "Error $? opening $ef: $!";
  my @tests = ();
  for my $line (<ERROR_FILE>) {
    chomp($line);
    if ($line =~ s/^CURRENT_TEST://) {
      push(@tests, $line);
    }
  }
  my $test_count = scalar(@tests);
  if ($test_count <= 20) {
    print("$test_count tests executed in this mtr thread:");
  } else {
    print("$test_count tests executed in this mtr thread; showing the last 20:");
    @tests = @tests[$test_count-20..$test_count];
  }
  print(@tests);
  print("\n");
  close ERROR_FILE or die "Error $? closing $ef: $!";
EOF

--echo last error log entry:$_log_errors

if ($_rpl_errors) {
  --echo rpl error summary:$_rpl_errors
}
if (!$_rpl_errors) {
  --echo rpl error summary: no errors in workers, receivers, or coordinators
}

if ($crash_after_show_rpl_debug_info) {
 --source include/dbug_crash_all.inc
}

--connection $_rpl_old_con
--enable_abort_on_error
