# Scenario 1
# Ensure we do not allow recovery in read-only mode.
#
# Prepare non empty redo log and crash.
SET GLOBAL innodb_redo_log_capacity = 10*1024*1024;
SET GLOBAL innodb_monitor_enable = module_log;
SET GLOBAL innodb_page_cleaner_disabled_debug = 0;
SET GLOBAL innodb_checkpoint_disabled = 1;
CREATE TABLE t (a INT PRIMARY KEY) ENGINE = INNODB;
INSERT INTO t (a) VALUES (1);
Expecting 1 as result of below query:
SELECT COUNT > 0 FROM INFORMATION_SCHEMA.INNODB_METRICS
WHERE NAME = 'log_lsn_checkpoint_age';
COUNT > 0
1
# Kill the server
# Restart MySQL in read-only mode (the same redo capacity).
Pattern "Can.t initiate database recovery, running in read-only-mode" found
# Cleanup...
# restart
DROP TABLE t;
# Scenario 2
# Ensure we do not allow to resize redo log in read-only mode.
#
# Perform slow shutdown to ensure redo is empty.
SET GLOBAL innodb_fast_shutdown = 0;
# Restart MySQL in read-only mode (different redo log capacity).
Pattern "Cannot resize redo log files in read-only mode" found
# Scenario 3
# Refuse to start in read-only mode without log files.
#
# Remove all redo log files.
Pattern "Cannot create redo log files in read-only mode" found
# Cleanup...
# restart
