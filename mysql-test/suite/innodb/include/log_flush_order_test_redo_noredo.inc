# Scenario "redo_noredo" - the same page is dirtied in redo mtr and then
# dirted in no-redo mtr and then again dirtied in redo mtr.
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

if ($debug_test) {
  --source ../include/log_read_dirty_pages_added_up_to_lsn.inc
  --source ../include/log_read_current_lsn.inc
  --echo Dirty pages added up to lsn: $dirty_pages_added_up_to_lsn
  --echo Current LSN: $current_lsn
}

--echo #
--echo # 2. Insert 20 rows.
--echo #

CALL mtr_redo_generate_multi(1, 10000, 20);

if ($debug_test) {
  --source ../include/log_read_current_lsn.inc
  --echo Current lsn: $current_lsn
}

--echo #
--echo # 3. Delete last 10 rows.
--echo #

DELETE FROM t1 ORDER BY a DESC LIMIT 10;

if ($debug_test) {
  --source ../include/log_read_current_lsn.inc
  --echo Current lsn: $current_lsn
}

--echo #
--echo # 4. Flush all dirty pages.
--echo #

SET GLOBAL innodb_buf_flush_list_now = 1;

--echo #
--echo # 5. CREATE INDEX to re-use the redo-dirtied pages within no-redo mtr...
--echo #

connect(con_index, localhost, root,,);
SET DEBUG_SYNC = "buf_flush_insert_remodified SIGNAL waiting WAIT_FOR go";
--send CREATE INDEX b_idx ON t1 (b);

--connection default

#SET DEBUG_SYNC = "now WAIT_FOR waiting";

if ($debug_test) {
  --source ../include/log_read_current_lsn.inc
  --echo Current lsn: $current_lsn
}

--echo #
--echo # 6. OK
--echo #

SET DEBUG_SYNC = "now SIGNAL go";
