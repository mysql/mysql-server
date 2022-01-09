# Prepare schema used in the tests.
CREATE TABLE t (a INT) ENGINE=InnoDB;
# Ensure there is a lot of free space in the redo log.
SET GLOBAL innodb_log_checkpoint_now = ON;
SET GLOBAL DEBUG = '+d,log_force_consumption';
SET GLOBAL DEBUG = '+d,syncpoint_log_before_waiting_for_next_file';
# Freeze log files consumer.
CALL mtr.add_suppression("Redo log writer is waiting for a new redo log file.*");;
SET GLOBAL DEBUG = '+d,syncpoint_log_before_file_consume';
# Wait until log files consumer is frozen.
SET DEBUG_SYNC = 'now WAIT_FOR reached_log_before_file_consume';
# Consumer is frozen. Create connection which generates spam to the redo log.
CALL log_spammer();
# Redo log data is being generated.
# Wait until log_writer signals it is waiting for a next file available.
SET DEBUG_SYNC = 'now WAIT_FOR reached_log_before_waiting_for_next_file';
SET GLOBAL DEBUG = '-d,syncpoint_log_before_waiting_for_next_file';
SET DEBUG_SYNC = 'now SIGNAL continue_log_before_waiting_for_next_file';
# The log_writer is waiting because there are no free files and consumer is frozen.
# Check that error log received information about that.
Pattern "Redo log writer is waiting for a new redo log file" found
# Unfreeze the log file consumer and wait until next log file is produced.
SET GLOBAL DEBUG = '+d,syncpoint_log_after_file_produced';
SET GLOBAL DEBUG = '-d,syncpoint_log_before_file_consume';
SET DEBUG_SYNC = 'now SIGNAL continue_log_before_file_consume';
SET DEBUG_SYNC = 'now WAIT_FOR reached_log_after_file_produced';
SET GLOBAL DEBUG = '-d,syncpoint_log_after_file_produced';
SET DEBUG_SYNC = 'now SIGNAL continue_log_after_file_produced';
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
