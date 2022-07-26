CREATE TABLE t1(a INT PRIMARY KEY) ENGINE=InnoDB;
BEGIN;
INSERT INTO t1 VALUES (42);
# Kill and restart: --innodb-redo-log-capacity=12M
SELECT * FROM t1;
a
INSERT INTO t1 VALUES (42);
BEGIN;
DELETE FROM t1;
# Kill and restart: --innodb-redo-log-capacity=15M
SELECT * FROM t1;
a
42
INSERT INTO t1 VALUES (123);
SET GLOBAL innodb_log_checkpoint_now = 1;
# Kill the server
#
# Crashing during recovery (initial phase)
#
# 1. Crash right before recovery starts:
Pattern "Completed space ID check" found
# 2. Crash right before log records are applied:
Pattern "Starting to parse redo log at lsn = [0-9]+, whereas checkpoint_lsn = [0-9]+" found
# 3. Crash right before recovery ends:
Pattern "Apply batch completed" found

#
# Crashing during upgrade (next phase after recovery)
#
# 1. Crash at the beginning of upgrade:
# 2. Crash right after flushing dirty pages:
Pattern "Apply batch completed" found
Pattern "Upgrading redo log: .* LSN=[0-9]+" found
# 3. Crash right after flushing lsn to system tablespace:
# 4. Crash right after deleting first log file:
# 5. Crash right after deleting all log files:
# 6. Crash after creating #ib_redo0_tmp:
# 7. Crash after renaming #ib_redo0_tmp to #ib_redo0:
# 8. Crash after creating all log files:
# 9. Crash after writing first checkpoint:
# 10. Crash after marking redo files as initialized:

#
# Start MySQL after upgrade.
#
# restart: --innodb-redo-log-capacity=15M
