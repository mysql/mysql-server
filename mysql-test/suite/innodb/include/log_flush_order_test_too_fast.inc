# Scenario "too_fast" - no-redo dirtied page inserted too quickly.
#

--echo #
--echo # 0. Block log.recent_closed.tail() from advancing by stopping single mtr
--echo #    just before it inserts its dirty pages to flush list.
--echo #

--source ../include/log_recent_closed_freeze.inc

--source ../include/log_read_dirty_pages_added_up_to_lsn.inc
--source ../include/log_read_current_lsn.inc

if ($debug_test) {
  --echo Dirty pages added up to lsn: $dirty_pages_added_up_to_lsn
  --echo Current LSN: $current_lsn
}

--echo #
--echo # 1. Optionally modify no-redo dirtied page (then it is later remodified).
--echo #

if ($noredo_remodify) {
  CALL mtr_noredo_generate_single();
  CALL mtr_noredo_generate_single();
}

let $init_lsn = $current_lsn;

--echo #
--echo # 2. Gather threads that try to insert dirtied pages with underlying redo
--echo #    records that protect the changes.
--echo #

let $i = $n_conn;
while ($i > 0) {
  --echo Connection $i starts inserting rows...

  connect(con$i, localhost, root,,);

  eval SET @q = CONCAT("CALL mtr_redo_generate_multi(", $i, ", 10000, 16)");
  PREPARE stmt FROM @q;
  --send EXECUTE stmt;

  dec $i;
}
--connection default

--echo #
--echo # 3. Wait gathering such threads until more data is written to log buffer.
--echo #    Wait until current lsn is greater than log.recent_closed.tail() by
--echo #    more than capacity of log.recent_closed.
--echo #

let $wait_lsn = $init_lsn + 2*1024*1024 + 10;

if ($debug_test) {
  --echo Wait LSN: $wait_lsn...
}

--source ../include/log_wait_for_current_lsn.inc

--source ../include/log_read_dirty_pages_added_up_to_lsn.inc
--source ../include/log_read_current_lsn.inc

if ($debug_test) {
  --echo Dirty pages added up to lsn: $dirty_pages_added_up_to_lsn
  --echo Current LSN: $current_lsn
}

--let $assert_cond = $current_lsn - $dirty_pages_added_up_to_lsn > 2*1024*1024
--let $assert_text = We must advance current lsn by more than maximum flush order lag.
--source include/assert.inc

--echo #
--echo # 4. Execute mini transactions with no-redo dirtied pages (MTR_LOG_NO_REDO).
--echo #
--echo #    When bug was present, such mtr inserted pages with too big lsn using
--echo #    current lsn, which does not fit space in log.recent_closed, bypassing
--echo #    required wait for space there (thanks to MTR_LOG_NO_REDO).
--echo #

if ($noredo_remodify == 1) {
  CALL mtr_noredo_update_all();
}
if ($noredo_remodify == 0) {
  CALL mtr_noredo_generate_single();
  CALL mtr_noredo_generate_single();
}

--echo #
--echo # 5. Resume the old mtr which blocked the log.recent_closed.tail() from advancing.
--echo #

--source ../include/log_recent_closed_unfreeze.inc

--echo #
--echo # 6. Finish pending insertions.
--echo #

let $i = $n_conn;
while ($i > 0) {
  --connection con$i
  reap;
  disconnect con$i;
  dec $i;
}
--connection default


--echo #
--echo # 7. Perform standard mtrs, just to ensure all is fine.
--echo #

CALL mtr_redo_generate_single(1, 10000);
CALL mtr_redo_generate_single(2, 10000);
