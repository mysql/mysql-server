# ==== Purpose ====
#
# Used by test `binlog_gtid.binlog_gtid_recovery_errors`, it sets up the
# scenario for a malformed binary log file to be read during recovery, by:
#   1. Getting the current binary log file name.
#   2. Crashing the server.
#   3. Replacing the current binlog file with a malformed binary log file,
#      tampered to trigger the given error scenario,
#   4. Restarting the server.
#
# ==== Parameters ====
#
#   $recovery_scenario
#     The recovery scenario being executed.
#
# ==== References ====
#
# WL#11300: Crash-safe XA + binary log
#
# Related tests;
#   see common/xa_crash_safe/setup.inc
#
if ($recovery_scenario == '') {
  --die ERROR: 'recovery_scenario' parameter needed by 'setup_scenario.inc'
}

--echo common/binlog/recovery/kill_copy_malformed_restart.inc [$recovery_scenario]

--let $datadir = `SELECT @@GLOBAL.datadir`
--let $last_binlog_file = query_get_value(SHOW BINARY LOG STATUS, File, 1)
--let $target_binlog_file = $datadir$last_binlog_file
--let $target_binlog_file_bak = $target_binlog_file.bak

--source include/kill_mysqld.inc

--let $malformed_binlog_file = $MYSQL_TEST_DIR/std_data/binlog_recovery/$recovery_scenario.000001
--copy_file $target_binlog_file $target_binlog_file_bak
--remove_file $target_binlog_file
--copy_file $malformed_binlog_file $target_binlog_file

--let $restart_parameters = restart: --debug=+d,eval_force_bin_log_recovery
--let $expect_crash_on_start = 1
--source include/start_mysqld.inc
