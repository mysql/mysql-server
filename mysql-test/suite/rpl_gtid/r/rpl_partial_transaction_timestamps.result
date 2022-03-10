include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
include/start_slave_io.inc
SET @save_debug=@@global.debug;
SET GLOBAL DEBUG= "+d,stop_io_after_reading_write_rows_log_event";
[connection master]
CREATE TABLE t1 (c1 INT) ENGINE=InnoDB;
BEGIN;
INSERT INTO t1 (c1) VALUES (1);
COMMIT;
[connection slave]
include/wait_for_slave_io_to_stop.inc
# Kill and restart
include/rpl_reconnect.inc
include/start_slave.inc
[connection master]
include/sync_slave_io_with_master.inc
include/assert.inc [Assert that the timestamp after the restart is more recent than the initial timestamp]
include/assert.inc [Assert that the final queue timestamp is more recent than the timestamp after the restart]
[connection master]
DROP TABLE t1;
include/rpl_end.inc
