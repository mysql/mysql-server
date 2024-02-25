CREATE TABLE t(a INT PRIMARY KEY, b INT);
INSERT INTO t(a, b) VALUES(1, 1);
SET GLOBAL innodb_log_checkpoint_now = 1;
SET GLOBAL DEBUG = 'd,log_force_consumption';
# Wait until redo occupies no more than 1MB on disk
# Resize redo log to 100M:
SET GLOBAL innodb_redo_log_capacity = 100*1024*1024;
# Create connection which generates spam to the redo log.
CALL log_spammer();
# Wait until redo occupies at least 10MB on disk
# Kill and restart mysql allowing initially big redo log:
# Kill and restart: --innodb-redo-log-capacity=104857600
# Initial state:
SELECT variable_name, variable_value FROM performance_schema.global_status
WHERE variable_name = 'Innodb_redo_log_capacity_resized';
variable_name	variable_value
Innodb_redo_log_capacity_resized	104857600
SET GLOBAL DEBUG = '+d,log_force_consumption';
# Waiting until we have exactly 1 redo log file.
There are <= 1 non-temporary redo log files.
SET GLOBAL DEBUG = '-d,log_force_consumption';
# Resize redo log to 8M:
SET GLOBAL innodb_redo_log_capacity = 8*1024*1024;
# Waiting for status = OK....
# Current state:
SELECT variable_name, variable_value FROM performance_schema.global_status
WHERE variable_name = 'Innodb_redo_log_capacity_resized';
variable_name	variable_value
Innodb_redo_log_capacity_resized	8388608
Checking if total size of redo log files does not exceed 8388608 bytes
Pattern "Truncating redo log file" found
UPDATE t SET b = 2 WHERE a = 1;
CALL mtr.add_suppression("The last block of redo had corrupted first_rec_group");
# Kill and restart:
SELECT * FROM t;
a	b
1	2
DROP TABLE t;
