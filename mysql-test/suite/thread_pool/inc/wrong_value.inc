--disable_query_log
--eval CALL mtr.add_suppression("$error_pattern")
--enable_query_log

# Code inspired by some solution found in binlog_stm_unsafe_warning.test.
let $log_error_= `SELECT @@GLOBAL.log_error`;
if($log_error_ == "stderr")
{
  # MySQL Server on windows is started with --console and thus
  # does not know the location of its .err log, use default location
  let $log_error_ = $MYSQLTEST_VARDIR/log/mysqld.1.err;
}

--echo # Check for expected content in mysqld.1.err

# Setup log file for assert_grep.inc
--let $assert_file= $log_error_

if ($tp_init_failure_expected)
{
  --eval CALL mtr.add_suppression("$TP_INIT_ERROR_PATTERN")

  --let $assert_text= "$TP_INIT_ERROR_PATTERN found"
  --let $assert_select= $TP_INIT_ERROR_PATTERN
  --let $assert_count= 1
  --source include/assert_grep.inc

  # Assert that the thread pool is not loaded if init failed.
  --error ER_UNKNOWN_SYSTEM_VARIABLE
  SELECT @@global.thread_pool_size;
}

if (!$tp_init_failure_expected)
{
  # Assert that the thread pool is loaded despite the wrong values.
  --assert (`SELECT @@global.thread_pool_size <> 0`)
}

--let $assert_text= $error_pattern found
--let $assert_select= $error_pattern
--let $assert_count= 1
--source include/assert_grep.inc
