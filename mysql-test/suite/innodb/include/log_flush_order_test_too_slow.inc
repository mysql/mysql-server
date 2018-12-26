# Scenario "too_slow" - no-redo dirtied page inserted too late.
#

let $i = $n_conn;
while ($i > 0) {
  connect(con$i, localhost, root,,);
  dec $i;
}

--connection default

--echo #
--echo # 0. Freeze single no-redo mtr (MTR_LOG_NO_REDO) in mtr0mtr.cc, just
--echo #    before it inserts the dirty page to flush list.
--echo #

BEGIN;
DELETE FROM tmp_t WHERE a < 0;

SET DEBUG = "+d,btr_ins_pause_on_mtr_noredo_before_add_dirty_blocks";

--send CALL mtr_noredo_generate_single();

connect(con_mgr, localhost, root,,);

SET DEBUG_SYNC = "now WAIT_FOR btr_ins_paused";

--source ../include/log_read_current_lsn.inc
let $init_lsn = $current_lsn;

if ($debug_test) {
  --echo Current LSN: $current_lsn
}

--echo #
--echo # 1. Allow user threads to execute many redo-based mini transactions,
--echo #    until current lsn is greater than init_lsn by more than capacity
--echo #    of log.recent_closed.
--echo #

let $i = $n_conn;
while ($i > 0) {
  --connection con$i
  BEGIN;
  eval DELETE FROM t$i WHERE a < 0;
  dec $i;
}

let $i = $n_conn;
while ($i > 0) {
  --connection con$i

  eval SET @q = CONCAT("CALL mtr_redo_generate_multi(", $i, ", 1800, 120)");
  PREPARE stmt FROM @q;
  --send EXECUTE stmt;

  dec $i;
}
--connection con_mgr

let $wait_lsn = $init_lsn + 2*1024*1024 + 10;
--source ../include/log_wait_for_current_lsn.inc

--echo #
--echo # 2. Optionally flush all dirty pages.
--echo #

if ($empty_flush_list) {
  --echo Flush all dirty pages...
  SET GLOBAL innodb_buf_flush_list_now = 1;
}

--echo #
--echo # 3. Resume the frozen no-redo mtr which seems to be too old.
--echo #

SET DEBUG_SYNC = "now SIGNAL btr_ins_resume";

--connection default
reap;
SET DEBUG = "-d,btr_ins_pause_on_mtr_noredo_before_add_dirty_blocks";
SET DEBUG_SYNC = "mtr_noredo_before_add_dirty_blocks CLEAR";
COMMIT;
# Just to clear the signal btr_ins_resume:
SET DEBUG_SYNC = "now WAIT_FOR btr_ins_resume";

disconnect con_mgr;

--echo #
--echo # 4. Finish pending insertions in redo-based mini transactions.
--echo #

let $i = $n_conn;
while ($i > 0) {
  --connection con$i
  reap;
  COMMIT;
  disconnect con$i;
  dec $i;
}
--connection default

--echo #
--echo # 5. Execute two standard mini transactions to ensure all is fine.
--echo #

CALL mtr_redo_generate_single(1, 1000);
CALL mtr_noredo_generate_single();
CALL mtr_redo_generate_single(2, 1000);
