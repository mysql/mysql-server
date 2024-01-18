# ==== Purpose ====
#
# Used by test `binlog_gtid.binlog_gtid_recovery_errors`, it cleans up
# after a binlog recovery error scenario, by:
#   1. Reinstating the correct binary log file.
#   2. Starting the crashed server.
#   3. Cleaning up server context.
#
# ==== Parameters ====
#
#   $recovery_scenario
#     The recovery scenario being executed.
#
#   $end_scenario_stmt
#     The statement to execute after restarting the server and before
#     cleaning the sever context.
#
#   $target_binlog_file
#     The active binlog file name, as instantiated in
#     `common/binlog/recovery/kill_copy_malformed_restart.inc`.
#
#   $target_binlog_file_bak
#     Backup of the binlog file before overwriting with a corrupted one, as
#     instantiated in
#     `common/binlog/recovery/kill_copy_malformed_restart.inc`.
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

--echo common/binlog/recovery/end_scenario.inc [$recovery_scenario]

--move_file $target_binlog_file_bak $target_binlog_file

--let $expect_crash_on_start =
--let $restart_parameters =
--source include/start_mysqld.inc

if ($end_scenario_stmt != '') {
  --eval $end_scenario_stmt
  --let $end_scenario_stmt =
}

DELETE FROM t1;
RESET BINARY LOGS AND GTIDS;
