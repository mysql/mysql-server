# Scenario "too_fast and empty flush list" - no-redo dirtied page inserted
# too quickly, and the flush list is empty when it happens.
#

--echo #
--echo # 0. Block log.recent_closed.tail() from advancing by stopping single mtr
--echo #    just before it inserts its dirty pages to flush list.
--echo #

let $recent_closed_freeze_empty_flush_list = 1;
--source ../include/log_recent_closed_freeze.inc
let $recent_closed_freeze_empty_flush_list = 0;

--echo #
--echo # 1. Frozen recent_closed.
--echo #

--source ../include/log_read_dirty_pages_added_up_to_lsn.inc

if ($debug_test) {
  --source ../include/log_read_current_lsn.inc
  --echo Dirty pages added up to lsn: $dirty_pages_added_up_to_lsn
  --echo Current LSN: $current_lsn
}

--echo #
--echo # 2. Optionally modify no-redo dirtied page (in which case next time we remodify it)
--echo #    Note it will be flushed mean while anyway.
--echo #

if ($noredo_remodify) {
  CALL mtr_noredo_generate_single();
  CALL mtr_noredo_generate_single();
}

--echo #
--echo # 3. Keep modifying pages, stop just before we reach limit in recent_closed.
--echo #

CALL mtr_redo_generate_multi(1, 10000, 150);

let $wait_lsn = $dirty_pages_added_up_to_lsn + 2*1024*1024 - 600;

let $stop = 0;
while ($stop == 0) {

  CALL mtr_redo_generate_single(1, 200);
  --source ../include/log_read_current_lsn.inc

  --let $cond = SELECT $current_lsn >= $wait_lsn;

  let $stop = `$cond`;
}


if ($debug_test) {
  --source ../include/log_read_dirty_pages_added_up_to_lsn.inc
  --source ../include/log_read_current_lsn.inc
  --echo Dirty pages added up to lsn: $dirty_pages_added_up_to_lsn
  --echo Current LSN: $current_lsn
}

--echo #
--echo # 4. Flush all dirty pages to make flush list empty.
--echo #

SET GLOBAL DEBUG = "+d,buf_flush_list_ignore_recent_closed";
SET GLOBAL innodb_buf_flush_list_now = 1;
SET GLOBAL DEBUG = "-d,buf_flush_list_ignore_recent_closed";

--echo #
--echo # 5. Execute multiple mtr, stopping them before they add any pages...
--echo #

let $i = $n_conn;
while ($i) {
  connect(con$i, localhost, root,,);

  # Force to start transaction and assign trx->id.
  # This can trigger flush of max_trx_id which can
  # involve its own mtr and result in dirty page.
  START TRANSACTION WITH CONSISTENT SNAPSHOT;

  SET DEBUG_SYNC = "mtr_redo_before_add_dirty_blocks WAIT_FOR mtr_go NO_CLEAR_EVENT";

  # This actually stops in mini-transaction related to undo_insert
  # record (50 bytes of redo). For 16 connections that gives 800 bytes.

  eval SET @q = CONCAT("CALL mtr_redo_generate_single(", $i, ", 0)");
  PREPARE stmt FROM @q;
  --send EXECUTE stmt;

  dec $i;
}
--connection default

--echo #
--echo # 6. Wait until we run out of free space in recent_closed...
--echo #

let $wait_lsn = $dirty_pages_added_up_to_lsn + 2*1024*1024 + 10;

if ($debug_test) {
  --echo Wait LSN: $wait_lsn
}

--source ../include/log_wait_for_current_lsn.inc

--echo #
--echo # 7. Execute mini transactions with no-redo dirtied pages (MTR_LOG_NO_REDO).
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
--echo # 8. Resume the old mtr which blocked the log.recent_closed.tail() from advancing.
--echo #

--source ../include/log_recent_closed_unfreeze.inc

--echo #
--echo # 9. Finish pending mini transactions
--echo #

SET DEBUG_SYNC = "now SIGNAL mtr_go";

let $i = $n_conn;
while ($i) {
  --connection con$i
  reap;
  COMMIT;
  disconnect con$i;
  dec $i;
}

--connection default

--echo #
--echo # 10. Perform standard mtrs, just to ensure all is fine.
--echo #

CALL mtr_redo_generate_single(1, 10000);
CALL mtr_redo_generate_single(2, 10000);
