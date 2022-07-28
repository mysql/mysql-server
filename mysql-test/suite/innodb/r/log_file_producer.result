# Prepare schema used in the tests.
CREATE TABLE t (a INT) ENGINE=InnoDB;
# Ensure there is a lot of free space in the redo log.
SET GLOBAL innodb_log_checkpoint_now = ON;
# Disable checkpointing
SET GLOBAL innodb_checkpoint_disabled = ON;
SET GLOBAL DEBUG = '+d,log_force_consumption';
# Pause any new log file productions
SET GLOBAL DEBUG = '+d,syncpoint_log_before_file_produced';
# Get notified when a new log file is produced (since now).
SET GLOBAL DEBUG = '+d,syncpoint_log_after_file_produced';
# Waiting until we have exactly 1 redo log file.
There are <= 1 non-temporary redo log files.
# Create connection which generates spam to the redo log.
CALL log_spammer();
# Redo log data is being generated.
# Wait until few log files are produced.
SET DEBUG_SYNC = 'now SIGNAL continue_log_before_file_produced WAIT_FOR reached_log_before_file_produced';
SET DEBUG_SYNC = 'now SIGNAL continue_log_after_file_produced WAIT_FOR reached_log_after_file_produced';
SET DEBUG_SYNC = 'now SIGNAL continue_log_before_file_produced WAIT_FOR reached_log_before_file_produced';
SET DEBUG_SYNC = 'now SIGNAL continue_log_after_file_produced WAIT_FOR reached_log_after_file_produced';
SET DEBUG_SYNC = 'now SIGNAL continue_log_before_file_produced WAIT_FOR reached_log_before_file_produced';
SET DEBUG_SYNC = 'now SIGNAL continue_log_after_file_produced WAIT_FOR reached_log_after_file_produced';
SET DEBUG_SYNC = 'now SIGNAL continue_log_before_file_produced WAIT_FOR reached_log_before_file_produced';
SET DEBUG_SYNC = 'now SIGNAL continue_log_after_file_produced WAIT_FOR reached_log_after_file_produced';
SET DEBUG_SYNC = 'now SIGNAL continue_log_before_file_produced WAIT_FOR reached_log_before_file_produced';
SET DEBUG_SYNC = 'now SIGNAL continue_log_after_file_produced WAIT_FOR reached_log_after_file_produced';
Redo files: 6
SET GLOBAL DEBUG = '-d,syncpoint_log_before_file_produced';
SET GLOBAL DEBUG = '-d,syncpoint_log_after_file_produced';
SET DEBUG_SYNC = 'now SIGNAL continue_log_before_file_produced,continue_log_after_file_produced';
# Create 1 row in t.
INSERT INTO t(a) VALUES(42);
# Kill the MySQL and recover to see all is fine
# Kill and restart
SELECT * FROM t;
a
42
# Cleanup.
SET DEBUG_SYNC = 'RESET';
DROP TABLE t;
