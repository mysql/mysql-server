CREATE TABLE t(a INT PRIMARY KEY, b INT);
INSERT INTO t(a, b) VALUES(1, 1);
# Resize redo log to 100M:
SET GLOBAL innodb_redo_log_capacity = 100*1024*1024;
SET GLOBAL innodb_log_checkpoint_now = 1;
# Create connection which generates spam to the redo log.
CALL log_spammer();
CALL log_spammer();
# Wait until redo occupies at least 10MB on disk
SET GLOBAL DEBUG = 'd,log_force_truncate';
# Resize redo log to 8M with usage of truncate (in background spammer keeps adding to redo!):
SET GLOBAL innodb_redo_log_capacity = 8*1024*1024;
# Waiting for status = OK....
Pattern "Truncating redo log file" found
UPDATE t SET b = 2 WHERE a = 1;
CALL mtr.add_suppression("The last block of redo had corrupted first_rec_group");
# Kill and restart
SELECT * FROM t;
a	b
1	2
DROP TABLE t;
