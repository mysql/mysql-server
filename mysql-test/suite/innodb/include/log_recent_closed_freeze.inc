connect(con_log_recent_closed_freeze, localhost, root,,);
# This table is removed when we unfreeze the log recent closed.
CREATE TABLE log_recent_closed_freeze_t (a INT PRIMARY KEY) ENGINE=InnoDB;

# Force to start transaction and assign trx->id.
# This can trigger flush of max_trx_id which can
# involve its own mtr and result in dirty page.
BEGIN;
DELETE FROM log_recent_closed_freeze_t WHERE a < 0;

if ($recent_closed_freeze_empty_flush_list) {
  SET GLOBAL innodb_buf_flush_list_now = 1;
}

--source ../include/log_read_current_lsn.inc
let $old_current_lsn = $current_lsn;
let $wait_lsn = $current_lsn;
--source ../include/buf_wait_for_dirty_pages_added_up_to_lsn.inc

SET DEBUG = "+d,btr_ins_pause_on_mtr_redo_before_add_dirty_blocks";
--send INSERT INTO log_recent_closed_freeze_t VALUES (1);

--connection default
SET DEBUG_SYNC = "now WAIT_FOR btr_ins_paused";
let $current_lsn = $old_current_lsn;
