# Enable all METRICS within log module
SET GLOBAL innodb_monitor_enable = module_log;

--source ../include/log_read_current_lsn.inc
--let $start_lsn = $current_lsn

SET GLOBAL DEBUG = "+d,trx_commit_to_the_end_of_log_block";

SET AUTOCOMMIT = 0;

eval $log_fill_block_sql;

if ($log_fill_block_and_crash == 1) {
  # Crash just before writing a next incomplete log block
  if ($debug_test) {
    --source ../include/log_read_current_lsn.inc
    --echo State: creating trx ending at blocks boundary
    --echo Start LSN: $start_lsn
    --echo Before COMMIT LSN: $current_lsn
  }

  SET GLOBAL DEBUG = "+d,crash_log_writer_before_write_new_incomplete_block";

  # We will crash during log writer's attempt to write 12 bytes of the header of the next block
  --source include/expect_crash.inc

  # COMMIT must not report it succeeded without writing the extra 12 bytes of the next block.
  --error 2013
}
COMMIT;

if ($log_fill_block_and_crash == 0) {
  SET GLOBAL DEBUG = "-d,trx_commit_to_the_end_of_log_block";

  --source ../include/log_read_current_lsn.inc
  --let $end_lsn = $current_lsn

  --let $assert_text = We failed to create log records that would end at boundary between blocks
  --let $assert_cond = [SELECT $end_lsn MOD 512] = 12
  --source include/assert.inc

  if ($debug_test) {
    --echo State: created trx ending at blocks boundary
    --echo Start LSN: $start_lsn
    --echo End LSN: $end_lsn
  }
}
