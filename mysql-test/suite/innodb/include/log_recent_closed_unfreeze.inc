# Release the blocked INSERT to log_recent_closed_freeze_t.
SET DEBUG_SYNC = "now SIGNAL btr_ins_resume";

# Wait until the INSERT is finished.
--connection con_log_recent_closed_freeze
reap;

# Do not block any further INSERT in this connection.
SET DEBUG = "-d,btr_ins_pause_on_mtr_redo_before_add_dirty_blocks";
SET DEBUG_SYNC = "mtr_redo_before_add_dirty_blocks CLEAR";

# Clear the signal (we were waiting with NO_CLEAR_EVENT in btr0cur.cc).
SET DEBUG_SYNC = "now WAIT_FOR btr_ins_resume";

# Commit the released INSERT (some redo log is generated).
COMMIT;

# Cleanup (again, some redo log is generated).
DROP TABLE log_recent_closed_freeze_t;
disconnect con_log_recent_closed_freeze;
--connection default
