# Initialize new data directory...
# Restart on the new data directory...
# restart: --datadir=tmp/log_corruption_1 --log-error=my_restart.err
# Create table and row...
CREATE TABLE t (a INT AUTO_INCREMENT PRIMARY KEY, b INT, INDEX b_idx(b))
ENGINE = InnoDB STATS_PERSISTENT = 0;
INSERT INTO t VALUES(1,1);
CHECK TABLE t;
Table	Op	Msg_type	Msg_text
test.t	check	status	OK
############################################################################################
# Case 1: Crash after redo logging information about corrupted clustered index.
############################################################################################
# Ensure there is a lot of free space in the redo log.
SET GLOBAL innodb_log_checkpoint_now = ON;
# Disable checkpointing.
SET GLOBAL innodb_checkpoint_disabled = ON;
# Enable crash code...
SET SESSION DEBUG = "+d,log_corruption_crash_1";
SET SESSION DEBUG = "+d,dict_set_index_corrupted";
# Run CHECK TABLE (expecting a crash)...
CHECK TABLE t;
# Verify that InnoDB can recover.
# restart: --datadir=tmp/log_corruption_1 --log-error=my_restart.err
Pattern "CORRUPT LOG RECORD FOUND" not found
CHECK TABLE t;
Table	Op	Msg_type	Msg_text
test.t	check	Warning	InnoDB: Index b_idx is marked as corrupted
test.t	check	error	Corrupt
ALTER TABLE t DROP INDEX b_idx;
CHECK TABLE t;
Table	Op	Msg_type	Msg_text
test.t	check	status	OK
ALTER TABLE t ADD INDEX b_idx (b);
############################################################################################
# Case 2: Crash after redo logging *corrupted* information about corrupted clustered index
#         (logging number of indexes corrupted = 100 > MAX_INDEXES).
############################################################################################
# Ensure there is a lot of free space in the redo log.
SET GLOBAL innodb_log_checkpoint_now = ON;
# Disable checkpointing.
SET GLOBAL innodb_checkpoint_disabled = ON;
# Enable crash code...
SET SESSION DEBUG = "+d,log_corruption_crash_1";
SET SESSION DEBUG = "+d,log_corruption_1";
SET SESSION DEBUG = "+d,dict_set_clust_index_corrupted";
# Run CHECK TABLE (expecting a crash)...
CHECK TABLE t;
# Verify that InnoDB cannot recover, because it encounters the corrupted redo record.
# Matching the server logs for corrupted redo logs...
Pattern "CORRUPT LOG RECORD FOUND" found
Pattern "Hex dump starting .* bytes before and ending .* bytes after the corrupted record" found
Pattern "innodb-unknown-compound-v" not found
############################################################################################
# Cleanup...
# restart:
