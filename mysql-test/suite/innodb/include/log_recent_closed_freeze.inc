connect(con_log_recent_closed_freeze, localhost, root,,);
CREATE TABLE log_recent_closed_freeze_t (a INT PRIMARY KEY) ENGINE=InnoDB;
if ($recent_closed_freeze_empty_flush_list) {
  SET GLOBAL innodb_buf_flush_list_now = 1;
}
let $old_current_lsn = $current_lsn;
--source ../include/log_read_current_lsn.inc
SET DEBUG_SYNC = CONCAT("mtr_redo_before_add_dirty_blocks ",
                        "SIGNAL log_recent_closed_frozen ",
                        "WAIT_FOR log_recent_closed_unfreeze ",
                        "NO_CLEAR_EVENT");
--send INSERT INTO log_recent_closed_freeze_t VALUES (1);
--connection default
SET DEBUG_SYNC = "now WAIT_FOR log_recent_closed_frozen";
let $wait_lsn = $current_lsn;
let $current_lsn = $old_current_lsn;
--source ../include/buf_wait_for_dirty_pages_added_up_to_lsn.inc
