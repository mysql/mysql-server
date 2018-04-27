# ==== Purpose ====
#
# Test one scenario using a combination of --expire-logs-days and
# --binlog-expire-logs-seconds. Verify that there are/aren't warnings
# as expected, and verify that the values of the global variables are
# as expected.
#
# ==== Usage ====
#
# --let $ofile = FILE
# --let $options = SERVER_OPTIONS
# --let $days = VALUE
# --let $seconds = VALUE
# --let $expect_deprecation_warning = [0|1]
# --let $expect_combination_warning = [0|1]
# --let $expect_binlog_off_and_days_warning = [0|1]
# --let $expect_binlog_off_and_seconds_warning = [0|1]
# --let $expect_seconds = VALUE
# --let $expect_days = VALUE
# --source include/binlog_expire_warnings.inc
#
# Parameters:
#
#  $ofile
#    Temporary file to use for the error log.
#
#  $options
#    Any additional options passed to mysqld during server start.
#
#  $days
#    The value to set for --expire_logs_days
#
#  $seconds
#    The value to set for --binlog-expire-logs-seconds
#
#  $expect_deprecation_warning
#    If zero, assert that there is no deprecation warning.
#    If nonzero, assert that there is a deprecation warning.
#
#  $expect_combination_warning
#    If zero, assert that there is no combination warning.
#    If nonzero, assert that there is a combination warning.
#
#  $expect_binlog_off_and_days_warning
#    If zero, assert that there is no warning due to using
#    --expire_logs_days with binlog disabled.
#    If nonzero, assert that there is a warning due to using
#    --expire_logs_days with binlog disabled.
#
#  $expect_binlog_off_and_seconds_warning
#    If zero, assert that there is no warning due to using
#    --binlog-expire-logs-seconds with binlog disabled.
#    If nonzero, assert that there is a warning due to using
#    --binlog-expire-logs-seconds with binlog disabled.
#
#  $expect_days
#    Assert that @@global.expire_logs_days has this value.
#
#  $expect_seconds
#    Assert that @@global.binlog_expire_logs_seconds has this value.


--let $restart_parameters = restart: --log-error=$ofile $options
if ($days != '') {
  --let $restart_parameters = $restart_parameters --expire_logs_days=$days
}
if ($seconds != '') {
  --let $restart_parameters = $restart_parameters --binlog-expire-logs-seconds=$seconds
}
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
--source include/restart_mysqld.inc


# For all the assert_grep.inc
--let $extra_debug_eval = LOAD_FILE("$ofile")


if (!$expect_deprecation_warning) {
  --let $assert_text = There shall be no deprecation warning
  --let $assert_count = 0
}
if ($expect_deprecation_warning) {
  --let $assert_text = There shall be a deprecation warning
  --let $assert_count = 1
}
--let $assert_file = $ofile
--let $assert_select = The syntax 'expire-logs-days' is deprecated and will be removed in a future release.
--let $assert_only_after = Shutdown complete
--source include/assert_grep.inc


if (!$expect_combination_warning) {
  --let $assert_text = There shall be no combination warning
  --let $assert_count = 0
}
if ($expect_combination_warning) {
  --let $assert_text = There shall be a combination warning
  --let $assert_count = 1
}
--let $assert_file = $ofile
--let $assert_select = The option expire_logs_days cannot be used together with option binlog_expire_logs_seconds. Therefore, value of expire_logs_days is ignored.
--let $assert_only_after = Shutdown complete
--source include/assert_grep.inc


if (!$expect_binlog_off_and_seconds_warning) {
  --let $assert_text = There shall be no binlog_off+seconds warning
  --let $assert_count = 0
}
if ($expect_binlog_off_and_seconds_warning) {
  --let $assert_text = There shall be a binlog_off+seconds warning
  --let $assert_count = 1
}
--let $assert_file = $ofile
--let $assert_select = You need to use --log-bin to make --binlog-expire-logs-seconds work.
--let $assert_only_after = Shutdown complete
--source include/assert_grep.inc


if (!$expect_binlog_off_and_days_warning) {
  --let $assert_text = There shall be no binlog_off+days warning
  --let $assert_count = 0
}
if ($expect_binlog_off_and_days_warning) {
  --let $assert_text = There shall be a binlog_off+days warning
  --let $assert_count = 1
}
--let $assert_file = $ofile
--let $assert_select = You need to use --log-bin to make --expire_logs_days work.
--let $assert_only_after = Shutdown complete
--source include/assert_grep.inc


# For all the assert.inc
--let $extra_debug_eval = CONCAT("days: ", @@global.expire_logs_days, " seconds: ", @@global.binlog_expire_logs_seconds)


--let $assert_text = binlog_expire_logs_seconds shall be $expect_seconds
--let $assert_cond = @@global.binlog_expire_logs_seconds = $expect_seconds
--source include/assert.inc


--let $assert_text = expire_logs_days shall be $expect_days
--let $assert_cond = @@global.expire_logs_days = $expect_days
--source include/assert.inc
