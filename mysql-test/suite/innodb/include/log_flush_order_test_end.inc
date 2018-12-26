--source ../include/log_read_current_lsn.inc
let $wait_lsn = $current_lsn;

if ($debug_test) {
  --echo Current LSN: $current_lsn
}

--source ../include/buf_wait_for_dirty_pages_added_up_to_lsn.inc

--source ../include/log_read_checkpoint_lsn.inc

if ($debug_test) {
  --echo Checkpoint LSN: $checkpoint_lsn
}

let $oldest_approx = `SELECT COUNT FROM INFORMATION_SCHEMA.INNODB_METRICS WHERE NAME = 'log_lsn_buf_pool_oldest_approx';`;

if ($debug_test) {
  --echo Oldest LSN approximation: $oldest_approx
}

--let $assert_cond = $oldest_approx = 0 OR $oldest_approx >= $checkpoint_lsn
--let $assert_text = We must not have oldest_lsn_approx < checkpoint_lsn.
--source include/assert.inc

SET GLOBAL innodb_checkpoint_disabled = 0;
SET GLOBAL innodb_log_checkpoint_fuzzy_now = 1;
SET GLOBAL innodb_checkpoint_disabled = 1;

if ($debug_test) {
  --source ../include/log_read_checkpoint_lsn.inc
  --echo New checkpoint LSN: $checkpoint_lsn
}

--source include/kill_and_restart_mysqld.inc
--disable_query_log
