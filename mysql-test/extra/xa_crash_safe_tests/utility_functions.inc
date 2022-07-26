# ==== Purpose ====
#
# Used by scripts testing XA crash recovery, it provides functions to be
# used with testing of crashes while executing XA transactions.
#
# ==== Parameters ====
#
# Check each function description.
#
# ==== References ====
#
# WL#11300: Crash-safe XA + binary log
#
# Related tests;
#   see extra/xa_crash_safe_tests/setup.inc
#

# The value for the table column, it will be incremented with each call to
# $exec_xa_transaction_body
#
--let $value_to_insert = 3

# ==== Purpose ====
#
# Executes an XA transaction up until after `XA END`. The XID and the
# connection used to execute the XA statements are dependent of the
# `$iteration` parameter, which will be used as a suffix for both.
#
# ==== Usage ====
#
# --let $iteration = [NUMBER]
# [--let $skip_init_connection = 0|1]
# --source $exec_xa_transaction_body
#
#   $iteration
#     Counter for the execution iteration step the caller is at. It will be
#     used as a suffix for both the XID and the connection on which the XA
#     transaction is executed
#
#   $skip_init_connection
#     Whether or not the conenction should be initialized. It's 0 by default.
#
--let $exec_xa_transaction_body = $MYSQLTEST_VARDIR/tmp/xa_crash_safe_tests_exec_xa_for_sync_point.inc
--write_file $exec_xa_transaction_body PROCEDURE
  if ($iteration == '') {
    --die ERROR: function $exec_xa_transaction_body needs 'interation' parameter
  }
  --let $init_connection = 1
  if ($skip_init_connection == 1) {
    --let $init_connection = 0
  }
  if ($init_connection == 1) {
    --connect(con$iteration, localhost, root,,)
  }
  --connection con$iteration
  --let $xid = 'xid$iteration'
  --eval XA START $xid
  --eval INSERT INTO t1 VALUES ($value_to_insert)
  --eval XA END $xid
  --inc $value_to_insert
#END OF
PROCEDURE

# ==== Purpose ====
#
# Rolls back XA transactions for which XID data is 'xid$suffix' and $start
# <= $suffix < $end.
#
# ==== Usage ====
#
# --let $start = [NUMBER]
# --let $end = [NUMBER]
# --source $clean_up_xa_transactions
#
#   $start
#     The integer value to start iterating from, while adding it as the
#     suffix of the XID to be rolled back.
#
#   $end
#     The integer value to stop iterating at, while adding it as the suffix
#     of the XID to be rolled back.
#
--let $clean_up_xa_transactions = $MYSQLTEST_VARDIR/tmp/xa_crash_safe_tests_clean_up_xa_transactions.inc
--write_file $clean_up_xa_transactions PROCEDURE
  if ($start == '') {
    --die ERROR: function $clean_up_xa_transactions needs 'start' index parameter
  }
  if ($stop == '') {
    --die ERROR: function $clean_up_xa_transactions needs 'stop' index parameter
  }
  --let $m = $start
  while ($m != $stop) {
    --let $xid = 'xid$m'
    --eval XA ROLLBACK $xid
    --inc $m
  }
#END OF
PROCEDURE

# ==== Purpose ====
#
# Tests that the server behaves as expected and outputs the expected error
# messages when an error is returned by SEs while processing normal
# transactions, during the server recovery.
#
# ==== Implementation ====
#
# Repeat for every possible error returned by SE during recovery:
# 1. Start a transaction.
# 2. Setup a conditional sync point for timestamp for the `COMMIT`
#    statement and execute up until that sync point.
# 3. Kill the server.
# 4. Restart server passing debug symbols that will simulate SEs returning
#    a given error and expect crash during start.
# 5. Find in the server log the error message corresponding to the
#    simulated error.
# 6. Restart the server without the debug symbols.
#
# ==== Usage ====
#
# --let $action = <commit|rollback>
# --let $sync_point = STRING
# --let $xa_errors = JSON_ARRAY
# --let $nth_iteration = INT
# --source $error_on_recovery_for_normal_transaction
#
#   $action
#     The action being taken when the server crashes.
#
#   $sync_point
#     The conditional sync point for timestamps to setup, for the server to
#     pause at, before being killed.
#
#   $xa_errors
#     The list of possible XA errors returned by SEs during transaction
#     recovery.
#
#   $nth_iteration
#     The nth iteration over testing scenarios, used to insert different
#     values on the test table.
#
--let $error_on_recovery_for_normal_transaction = $MYSQLTEST_VARDIR/tmp/xa_crash_safe_tests_error_on_recovery_for_normal_transaction.inc
--write_file $error_on_recovery_for_normal_transaction PROCEDURE
  if ($action == '') {
    --die ERROR: function $error_on_recovery_for_normal_transaction needs 'action' parameter with possible values [commit|rollback]
  }
  if ($sync_point == '') {
    --die ERROR: function $error_on_recovery_for_normal_transaction needs 'sync_point' parameter
  }
  if ($xa_errors == '') {
    --die ERROR: function $error_on_recovery_for_normal_transaction needs 'xa_errors' list of errors parameter
  }
  if ($nth_iteration == '') {
    --die ERROR: function $error_on_recovery_for_normal_transaction needs 'k' index parameter
  }

  --let $json_array = $xa_errors
  --source $json_xa_error_list_start
  while (!$json_xa_error_list_done) {
    --let $xa_error = $json_xa_error_list_value
    --let $xa_error_lc = `SELECT LOWER("$xa_error")`

    --connect(con1, localhost, root,,)
    --connection con1

    --eval BEGIN; INSERT INTO t1 VALUES ($nth_iteration)

    --let $auxiliary_connection = default
    --let $statement_connection = con1
    --let $statement = COMMIT
    --source include/execute_to_conditional_timestamp_sync_point.inc
    --source include/kill_mysqld.inc
    --source extra/xa_crash_safe_tests/cleanup_connection.inc

    --let $debug_point = xa_recovery_error_$xa_error_lc,xa_recovery_error_reporting
    --let $restart_parameters = restart: --debug=+d,$debug_point
    --let $expect_crash_on_start = 1
    --source include/start_mysqld.inc

    --let $grep_file = $MYSQLTEST_VARDIR/log/mysqld.1.err
    --let $grep_pattern = .*Failed to $action transaction .* in InnoDB, with failure code $xa_error.*
    --source include/wait_for_pattern_in_file.inc

    --let $expect_crash_on_start =
    --let $restart_parameters = restart:
    --source include/start_mysqld.inc

    --inc $nth_iteration
    --source $json_xa_error_list_next
  }
#END OF
PROCEDURE

# ==== Purpose ====
#
# Tests that the server behaves as expected and outputs the expected error
# messages when an error is returned by SEs while processing XA
# transactions, during the server recovery.
#
# ==== Implementation ====
#
# Repeat for every possible error returned by SE during recovery:
# 1. Start a transaction.
# 2. Setup a conditional sync point for timestamp for the statement
#    associated with the provided `$action` and execute up until that sync
#    point.
# 3. Kill the server.
# 4. Restart server passing debug symbols that will simulate SEs returning
#    a given error and expect crash during start.
# 5. Find in the server log the error message corresponding to the
#    simulated error.
# 6. Restart the server without the debug symbols.
#
# ==== Usage ====
#
# --let $action = <prepare|commit|rollback>
# --let $sync_point = STRING
# --let $xa_errors = JSON_ARRAY
# --let $nth_iteration = INT
# [--let $xa_opt = one phase]
# [--let $expected_recovery = <none|prepare|commit|rollback>]
# --source $error_on_recovery_for_normal_transaction
#
#   $action
#     The action being taken when the server crashes.
#
#   $sync_point
#     The conditional sync point for timestamps to setup, for the server to
#     pause at, before being killed.
#
#   $xa_errors
#     The list of possible XA errors returned by SEs during transaction
#     recovery.
#
#   $nth_iteration
#     The nth iteration over testing scenarios, used to insert different
#     values on the test table and to generate different XIDs.
#
#   $xa_opt
#     Optional parameter for the `XA COMMIT` statement (one phase).
#
#   $expected_recovery
#     Optional parameter for when the expected state after recovery is
#     different from the performed action:
#     - none: no action was attempted at recovery and no transaction
#       needing recovery was found.
#     - prepare: recovery should've attempted to prepare the transaction.
#     - commit: recovery should've attempted to commit the transaction.
#     - rollback: recovery should've attempted to rollback the transaction.
#
--let $error_on_recovery_for_xa_transaction = $MYSQLTEST_VARDIR/tmp/xa_crash_safe_tests_error_on_recovery_for_xa_transaction.inc
--write_file $error_on_recovery_for_xa_transaction PROCEDURE
  if ($action == '') {
    --die ERROR: function $error_on_recovery_for_xa_transaction needs 'action' parameter with possible values [prepare|commit|rollback]
  }
  if ($sync_point == '') {
    --die ERROR: function $error_on_recovery_for_xa_transaction needs 'sync_point' parameter
  }
  if ($xa_errors == '') {
    --die ERROR: function $error_on_recovery_for_xa_transaction needs 'xa_errors' list of errors parameter
  }
  if ($nth_iteration == '') {
    --die ERROR: function $error_on_recovery_for_xa_transaction needs 'k' index parameter
  }
    --let $_expected_recovery = $action
  if ($expected_recovery != '') {
    --let $_expected_recovery = $expected_recovery
  }

  --let $json_array = $xa_errors
  --source $json_xa_error_list_start
  while (!$json_xa_error_list_done) {
    --let $xa_error = $json_xa_error_list_value
    --let $xa_error_lc = `SELECT LOWER("$xa_error")`

    --connect(con1, localhost, root,,)
    --connection con1

    --let $xid = `SELECT CONCAT("X'", LOWER(HEX('blxre$nth_iteration')), "',X'',1")`
    --eval XA START $xid
    --eval INSERT INTO t1 VALUES ($nth_iteration)
    --eval XA END $xid
    if (`SELECT '$action' <> 'prepare' AND '$xa_opt' <> 'one phase'`) {
      --eval XA PREPARE $xid
    }

    --let $auxiliary_connection = default
    --let $statement_connection = con1
    --let $statement = XA $action $xid $xa_opt
    --source include/execute_to_conditional_timestamp_sync_point.inc
    --source include/kill_mysqld.inc
    --source extra/xa_crash_safe_tests/cleanup_connection.inc

    --let $debug_point = xa_recovery_error_$xa_error_lc,xa_recovery_error_reporting
    --let $restart_parameters = restart: --debug=+d,$debug_point
    --let $expect_crash_on_start = 1
    --source include/start_mysqld.inc

    if ($_expected_recovery != 'none') {
      --let $grep_file = $MYSQLTEST_VARDIR/log/mysqld.1.err
      --let $grep_pattern = .*Failed to $_expected_recovery XA transaction $xid in InnoDB, with failure code $xa_error.*
      --source include/wait_for_pattern_in_file.inc
    }

    --let $expect_crash_on_start =
    --let $restart_parameters =
    --source include/start_mysqld.inc

    if (`SELECT ('$action' = 'prepare' OR '$action' = 'commit') AND '$_expected_recovery' = 'prepare'`) {
      --eval XA COMMIT $xid
    }
    if (`SELECT '$action' = 'rollback' AND '$_expected_recovery' = 'prepare'`) {
      --eval XA ROLLBACK $xid
    }
    if ($_expected_recovery == 'none') {
      --let $expected_prepared_xa_count = 0
      --source extra/xa_crash_safe_tests/assert_xa_recover.inc
    }

    --inc $nth_iteration
    --source $json_xa_error_list_next
  }
#END OF
PROCEDURE
