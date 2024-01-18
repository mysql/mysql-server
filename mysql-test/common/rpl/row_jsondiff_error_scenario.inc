# ==== Purpose ====
#
# Check that slave handles an applier error according to
# replica_exec_mode, slave_skip_error, or none of them.
#
# ==== Usage ====
#
# --let $desc= TEXT
# [--let $first_case= N]
# --let $table= DB.TABLE
# --let $column_def= DEFINITION
# [--let $column_def_slave= DEFINITION]
# --let $rows= (comma-separated-values) [, (comma-separated-values) ...]
# [--let $rows_slave= (comma-separated-values) [, (comma-separated-values) ...]]
# [--let $insert_columns= (col1, col2, ...)]
# --let $stmt_pre= PREFIX
# --let $stmt= STMT
# --let $stmt_post= SUFFIX
# [--let $master_debug_symbol= SYMBOL]
# [--let $slave_debug_symbol= SYMBOL]
# --let $error= NUMBER
# --let $error_mode= { FAIL | SKIP | IDEMPOTENT }
# --source include/rpl/handle_applier_error.inc
#
# Invariants:
# - Replication threads should be running, and will be left running.
# - replica_exec_mode or slave_skip_error should be set according to $error_mode.
#
# Parameters:
#
#  $desc
#    Human-readable description of the scenario
#
#  $first_case
#    If set, will skip the first $first_case invocations of this script.
#    (For debugging.)
#
#  $table
#    Fully qualified name of table to operate on
#
#  $column_def
#    Column definitions for table, i.e., everything between opening
#    parenthesis and closing parenthesis in CREATE TABLE (...).
#
#  $column_def_slave
#    Different column definition for slave. If this is not set, slave
#    will use the same column definition as the master.
#
#  $rows
#    Row values to insert in the table.
#
#  $rows_slave
#    Different rows to insert in the slave's table. If this is not set,
#    slave will use the same values as master.
#
#  $insert_columns
#    Comma-separated, parenthesis-wrapped list of names of columns to
#    insert, used as in
#      INSERT INTO table (col1, col2, ...) VALUES ...
#
#  $stmt_pre, $stmt, $stmt_post
#    These strings will be concatenated, and the result executed as an
#    SQL statement on the master and replicated to slave. The
#    assumption is that it will generate an error in the applier
#    thread on the slave.
#
#  $master_debug_symbol
#    If set, this debug symbol will be added to @@global.debug on
#    master, before executing the statement on master.
#
#  $slave_debug_symbol
#    If set, this debug symbol will be added to @@global.debug on
#    slave, before executing the statement on master.
#
#  $error
#    Expected error code (numeric).
#
#  $error_mode
#    How error will be handled.  FAIL means the default mode where the
#    threads stop with an error.  SKIP means slave_skip_error is set
#    to the error and the slave is expected to continue.  IDEMPOTENT
#    means the slave is expected to skip duplicate-key and
#    key-not-found errors.
#
#  $recover_from_error
#    By default, does not attempt to recover from the error. If this
#    variable is set, assumes the error is due to a difference between
#    master and slave, and re-creates the table on slave as a copy of
#    that on the master, and then tries again.

if (!$case) {
  --let $case= 0
}
--inc $case
--let $skip_case= 0
if ($first_case) {
  if ($case < $first_case) {
    --let $skip_case= 1
  }
}
if ($last_case) {
  if ($case > $last_case) {
    --let $skip_case= 1
  }
}
--echo ==== $case. $desc ====

--let $include_filename= rpl/row_jsondiff_error_scenario [$error/$error_mode]
--source include/begin_include_file.inc

if (!$skip_case)
{
  #--disable_query_log

  --source include/rpl/connection_replica.inc

  --let $actual_error_mode= $error_mode
  if ($error_mode == IDEMPOTENT)
  {
    --let $actual_error_mode= FAIL
    if ($error == $er_dup_entry) {
      --let $actual_error_mode= SKIP
    }
    if ($error == $er_key_not_found) {
      --let $actual_error_mode= SKIP
    }
  }

  # Setup table on master
  --source include/rpl/connection_source.inc
  SET SQL_LOG_BIN = 0;
  eval CREATE TABLE $table ($column_def);
  eval INSERT INTO $table $insert_columns VALUES $rows;
  SET SQL_LOG_BIN = 1;

  # Setup table on slave
  --source include/rpl/connection_replica.inc
  --let $_column_def= $column_def
  if ($column_def_slave) {
    --let $_column_def= $column_def_slave
  }
  --let $_rows= $rows
  if ($rows_slave) {
    --let $_rows= $rows_slave
  }
  SET SQL_LOG_BIN = 0;
  eval CREATE TABLE $table ($_column_def);
  eval INSERT INTO $table $insert_columns VALUES $_rows;
  SET SQL_LOG_BIN = 1;
  if ($slave_debug_symbol) {
    --let $debug_point= $slave_debug_symbol
    --let $debug_type= GLOBAL
    --source include/add_debug_point.inc
    --source include/rpl/stop_replica.inc
    --source include/rpl/start_replica.inc
  }

  # Execute statement on master
  --source include/rpl/connection_source.inc
  if ($master_debug_symbol) {
    --let $debug_point= $master_debug_symbol
    --let $debug_type= SESSION
    --source include/add_debug_point.inc
  }
  if ($use_mysqlbinlog) {
    --source include/rpl/save_binlog_file_position.inc
  }
  --eval $stmt_pre $stmt $stmt_post

  if ($master_debug_symbol) {
    --let $debug_point= $master_debug_symbol
    --let $debug_type= SESSION
    --source include/remove_debug_point.inc
  }

  # Verify error and cleanup error
  if ($actual_error_mode == FAIL)
  {
    --source include/rpl/connection_replica.inc
    --let $slave_sql_errno= $error
    --source include/rpl/wait_for_applier_error.inc

    if ($use_mysqlbinlog) {
      --let $mysqlbinlog_only_decoded_rows= 1
      --source include/rpl/mysqlbinlog.inc
    }
    if ($slave_debug_symbol) {
      --let $debug_point= $slave_debug_symbol
      --let $debug_type= GLOBAL
      --source include/remove_debug_point.inc
    }

    if ($recover_from_error)
    {
      # Create table like on master
      SET SQL_LOG_BIN = 0;
      eval DROP TABLE $table;
      eval CREATE TABLE $table ($column_def);
      eval INSERT INTO $table $insert_columns VALUES $rows;
      SET SQL_LOG_BIN = 1;

      # Restart replication
      --source include/rpl/stop_receiver.inc
      RESET REPLICA;
      --source include/rpl/start_replica.inc

      --source include/rpl/connection_source.inc
      # Verify no error
      --source include/rpl/sync_to_replica.inc
    }
    if (!$recover_from_error)
    {
      --source include/rpl/stop_receiver.inc
      RESET REPLICA;
      --source include/rpl/connection_source.inc
      RESET BINARY LOGS AND GTIDS;
      --source include/rpl/connection_replica.inc
      --source include/rpl/start_replica.inc
    }
  }

  if ($actual_error_mode == SKIP)
  {
    # Verify no error
    --source include/rpl/sync_to_replica.inc

    if ($slave_debug_symbol) {
      --let $debug_point= $slave_debug_symbol
      --let $debug_type= GLOBAL
      --source include/remove_debug_point.inc
    }
  }

  # Clean up
  --source include/rpl/connection_source.inc
  eval DROP TABLE $table;
  --source include/rpl/sync_to_replica.inc
  --source include/rpl/stop_replica.inc

  # BUG#27636289: RPL BREAKS WITH RESTART AFTER RESET SLAVE,
  #               RESET BINARY LOGS AND GTIDS WITH WRONG ERROR IN MTS
  # Due to the above mentioned bug we need to use RESET REPLICA ALL
  # and CHANGE REPLICATION SOURCE instead of just RESET REPLICA, in order to clear
  # RECEIVED_GTID_SET when test runs in MTS mode.

  RESET REPLICA ALL;
  RESET BINARY LOGS AND GTIDS;
  --source include/rpl/connection_source.inc
  RESET BINARY LOGS AND GTIDS;
  --source include/rpl/connection_replica.inc
  --replace_result $MASTER_MYPORT SOURCE_PORT
  --disable_warnings
  eval CHANGE REPLICATION SOURCE TO SOURCE_USER='root',SOURCE_PORT=$MASTER_MYPORT,SOURCE_HOST='127.0.0.1';
  --enable_warnings
  --source include/rpl/start_replica.inc

}

--let $debug_point=

--source include/end_include_file.inc
