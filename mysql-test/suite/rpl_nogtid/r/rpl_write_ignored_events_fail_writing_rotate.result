include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
CALL mtr.add_suppression('Failed to create Rotate_event .out of memory');
CHANGE REPLICATION SOURCE TO IGNORE_SERVER_IDS = (1);
include/start_slave_io.inc
[connection master]
CREATE TABLE t1 (c1 INT);
DROP TABLE t1;
include/sync_slave_io_with_master.inc
SET @@GLOBAL.debug="+d,fail_generating_rotate_event_on_write_rotate_to_source_pos";
include/stop_slave_io.inc
SET @@GLOBAL.debug="-d,fail_generating_rotate_event_on_write_rotate_to_source_pos";
include/wait_for_slave_io_error.inc [errno=13116]
Last_IO_Error = 'Failed to create Rotate_event (out of memory?), SHOW SLAVE STATUS may be inaccurate'
CHANGE REPLICATION SOURCE TO IGNORE_SERVER_IDS = ();
include/start_slave.inc
[connection master]
CREATE TABLE t1 (c1 INT);
INSERT INTO t1 (c1) VALUES (1);
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
include/rpl_reset.inc
[connection slave]
include/stop_slave.inc
CALL mtr.add_suppression('failed to write a Rotate event to the relay log');
CHANGE REPLICATION SOURCE TO IGNORE_SERVER_IDS = (1);
include/start_slave_io.inc
[connection master]
CREATE TABLE t1 (c1 INT);
DROP TABLE t1;
include/sync_slave_io_with_master.inc
SET @@GLOBAL.debug="+d,fail_to_write_ignored_event_to_relay_log";
include/stop_slave_io.inc
SET @@GLOBAL.debug="-d,fail_to_write_ignored_event_to_relay_log";
include/wait_for_slave_io_error.inc [errno=13122]
Last_IO_Error = 'Relay log write failure: failed to write a Rotate event to the relay log, SHOW SLAVE STATUS may be inaccurate'
CHANGE REPLICATION SOURCE TO IGNORE_SERVER_IDS = ();
include/start_slave.inc
[connection master]
CREATE TABLE t1 (c1 INT);
INSERT INTO t1 (c1) VALUES (1);
DROP TABLE t1;
include/rpl_reset.inc
[connection slave]
include/stop_slave.inc
CALL mtr.add_suppression('Failed to flush master info file');
CHANGE REPLICATION SOURCE TO IGNORE_SERVER_IDS = (1);
include/start_slave_io.inc
[connection master]
CREATE TABLE t1 (c1 INT);
DROP TABLE t1;
include/sync_slave_io_with_master.inc
SET @@GLOBAL.debug="+d,fail_to_flush_source_info";
include/stop_slave_io.inc
SET @@GLOBAL.debug="-d,fail_to_flush_source_info";
CHANGE REPLICATION SOURCE TO IGNORE_SERVER_IDS = ();
include/start_slave.inc
[connection master]
CREATE TABLE t1 (c1 INT);
INSERT INTO t1 (c1) VALUES (1);
DROP TABLE t1;
include/rpl_end.inc
