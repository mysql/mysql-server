include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
# Initial setup
[connection master]
CREATE TABLE t1 (c1 INT) ENGINE=InnoDB;
# Sync slave with master and setup debug sync point
include/sync_slave_sql_with_master.inc
SET @save_debug=@@global.debug;
SET GLOBAL DEBUG='+d,stop_io_after_reading_write_rows_log_event';
# Insert the data in the master
[connection master]
BEGIN;
INSERT INTO t1 (c1) VALUES (0);
INSERT INTO t1 (c1) VALUES (1);
COMMIT;
# Restart the IO thread keeping SQL thread running until last relay log split
[connection slave]
include/wait_for_slave_io_to_stop.inc
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_to_stop.inc
# Stop the SQL thread (it should be in the middle of the transaction/group)
include/stop_slave_sql.inc
# Remove the debug point and restart the both threads
SET GLOBAL DEBUG= @save_debug;
include/start_slave.inc
# Do one more insert on master and then sync slave with master
[connection master]
INSERT INTO t1 (c1) VALUES (2);
include/sync_slave_sql_with_master.inc
# Now compare master and slave's t1 table data
[connection master]
include/diff_tables.inc [master:t1, slave:t1]
# Cleanup
[connection master]
DROP TABLE t1;
include/rpl_end.inc
