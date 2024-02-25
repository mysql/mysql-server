# Prepare schema used in the tests.
# Ensure there is a lot of free space in the redo log.
SET GLOBAL innodb_log_checkpoint_now = ON;
SET GLOBAL DEBUG="+d,log_force_consumption";
# Waiting until we have exactly 1 redo log file.
There are <= 1 non-temporary redo log files.
# Disable checkpointing
SET GLOBAL innodb_checkpoint_disabled = ON;
CREATE TABLE t (a INT) ENGINE=InnoDB;
# Create connection which generates spam to the redo log.
CALL log_spammer();
# Redo log data is being generated and redo is resized-up after each round.
"Waiting for physical size to reach 75% of"
SELECT @@innodb_redo_log_capacity;
@@innodb_redo_log_capacity
10485760
SET GLOBAL innodb_redo_log_capacity = @@innodb_redo_log_capacity * 2;
"Waiting for physical size to reach 75% of"
SELECT @@innodb_redo_log_capacity;
@@innodb_redo_log_capacity
20971520
SET GLOBAL innodb_redo_log_capacity = @@innodb_redo_log_capacity * 2;
"Waiting for physical size to reach 75% of"
SELECT @@innodb_redo_log_capacity;
@@innodb_redo_log_capacity
41943040
SET GLOBAL innodb_redo_log_capacity = @@innodb_redo_log_capacity * 2;
"Waiting for physical size to reach 75% of"
SELECT @@innodb_redo_log_capacity;
@@innodb_redo_log_capacity
83886080
SET GLOBAL innodb_redo_log_capacity = @@innodb_redo_log_capacity * 2;
# Create 1 row in t.
INSERT INTO t(a) VALUES(42);
# Enable checkpointing
SET GLOBAL innodb_checkpoint_disabled = OFF;
# Redo log is resized-down in single shot.
SET GLOBAL innodb_redo_log_capacity = 10485760;
Timeout in wait_condition.inc for SELECT variable_value <= @@innodb_redo_log_capacity/1024/1024
FROM performance_schema.global_status WHERE variable_name = 'Innodb_redo_log_physical_size'
# Kill the MySQL and recover to see all is fine
# Kill and restart
SELECT * FROM t;
a
42
# Cleanup
DROP TABLE t;
