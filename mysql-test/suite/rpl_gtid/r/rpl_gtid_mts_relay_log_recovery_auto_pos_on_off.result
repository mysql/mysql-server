include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
CALL mtr.add_suppression("Recovery from master pos*");
CALL mtr.add_suppression("Slave SQL.*Duplicate entry.* Error_code: MY-001062");
CALL mtr.add_suppression("The slave coordinator and worker threads are stopped, possibly leaving data in inconsistent state");
include/stop_slave.inc
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION=0;
SET @@global.replica_parallel_workers=4;
include/start_slave.inc
[connection master]
CREATE DATABASE d1;
CREATE DATABASE d2;
CREATE DATABASE d3;
CREATE DATABASE d4;
CREATE TABLE d1.t1 (a int unique) ENGINE=INNODB;
CREATE TABLE d2.t1 (a int unique) ENGINE=INNODB;
CREATE TABLE d3.t1 (a int unique) ENGINE=INNODB;
CREATE TABLE d4.t1 (a int unique) ENGINE=INNODB;
include/sync_slave_sql_with_master.inc
STOP SLAVE SQL_THREAD;
Warnings:
Warning	1287	'STOP SLAVE' is deprecated and will be removed in a future release. Please use STOP REPLICA instead
SET GLOBAL DEBUG='d,flush_after_reading_user_var_event';
BEGIN;
INSERT INTO d1.t1 VALUES (1);
INSERT INTO d2.t1 VALUES (2);
INSERT INTO d3.t1 VALUES (3);
[connection master]
INSERT INTO d1.t1 VALUES (4);
INSERT INTO d2.t1 VALUES (5);
INSERT INTO d3.t1 VALUES (6);
INSERT INTO d4.t1 VALUES (7);
[connection master1]
BEGIN;
INSERT INTO d1.t1 VALUES (1);
SET @v= 100;
INSERT INTO d1.t1 VALUES (@v);
INSERT INTO d2.t1 VALUES (2);
INSERT INTO d3.t1 VALUES (3);
[connection master]
BEGIN;
INSERT INTO d4.t1 VALUES (8);
[connection master1]
COMMIT;
[connection slave1]
SET DEBUG_SYNC='now WAIT_FOR Reached';
FLUSH LOGS;
# Let IO thread continue splitting has been done now.
SET DEBUG_SYNC= 'now SIGNAL signal.flush_complete_continue';
[connection master]
COMMIT;
INSERT INTO d4.t1 VALUES (9);
include/sync_slave_io_with_master.inc
[connection slave1]
START SLAVE SQL_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
[connection slave]
COMMIT;
include/wait_for_slave_sql_error.inc [errno=1062]
include/stop_slave_io.inc
DELETE FROM d1.t1 where a=1;
DELETE FROM d2.t1 where a=2;
DELETE FROM d3.t1 where a=3;
include/rpl_restart_server.inc [server_number=2 gtids=on parameters: --skip_replica_start=FALSE --sync_source_info=1 --relay-log-recovery=1]
[connection master]
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:d4.t1, slave:d4.t1]
include/diff_tables.inc [master:d3.t1, slave:d3.t1]
include/diff_tables.inc [master:d2.t1, slave:d2.t1]
include/diff_tables.inc [master:d1.t1, slave:d1.t1]
[connection master]
DROP DATABASE d1;
DROP DATABASE d2;
DROP DATABASE d3;
DROP DATABASE d4;
include/sync_slave_sql_with_master.inc
include/rpl_reset.inc
include/stop_slave.inc
CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION=1;
SET @@global.replica_parallel_workers=4;
include/start_slave.inc
[connection master]
CREATE DATABASE d1;
CREATE DATABASE d2;
CREATE DATABASE d3;
CREATE DATABASE d4;
CREATE TABLE d1.t1 (a int unique) ENGINE=INNODB;
CREATE TABLE d2.t1 (a int unique) ENGINE=INNODB;
CREATE TABLE d3.t1 (a int unique) ENGINE=INNODB;
CREATE TABLE d4.t1 (a int unique) ENGINE=INNODB;
include/sync_slave_sql_with_master.inc
STOP SLAVE SQL_THREAD;
Warnings:
Warning	1287	'STOP SLAVE' is deprecated and will be removed in a future release. Please use STOP REPLICA instead
SET GLOBAL DEBUG='d,flush_after_reading_user_var_event';
BEGIN;
INSERT INTO d1.t1 VALUES (1);
INSERT INTO d2.t1 VALUES (2);
INSERT INTO d3.t1 VALUES (3);
[connection master]
INSERT INTO d1.t1 VALUES (4);
INSERT INTO d2.t1 VALUES (5);
INSERT INTO d3.t1 VALUES (6);
INSERT INTO d4.t1 VALUES (7);
[connection master1]
BEGIN;
INSERT INTO d1.t1 VALUES (1);
SET @v= 100;
INSERT INTO d1.t1 VALUES (@v);
INSERT INTO d2.t1 VALUES (2);
INSERT INTO d3.t1 VALUES (3);
[connection master]
BEGIN;
INSERT INTO d4.t1 VALUES (8);
[connection master1]
COMMIT;
[connection slave1]
SET DEBUG_SYNC='now WAIT_FOR Reached';
FLUSH LOGS;
# Let IO thread continue splitting has been done now.
SET DEBUG_SYNC= 'now SIGNAL signal.flush_complete_continue';
[connection master]
COMMIT;
INSERT INTO d4.t1 VALUES (9);
include/sync_slave_io_with_master.inc
[connection slave1]
START SLAVE SQL_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
[connection slave]
COMMIT;
include/wait_for_slave_sql_error.inc [errno=1062]
include/stop_slave_io.inc
DELETE FROM d1.t1 where a=1;
DELETE FROM d2.t1 where a=2;
DELETE FROM d3.t1 where a=3;
include/rpl_restart_server.inc [server_number=2 gtids=on parameters: --skip_replica_start=FALSE --sync_source_info=1 --relay-log-recovery=1]
[connection master]
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:d4.t1, slave:d4.t1]
include/diff_tables.inc [master:d3.t1, slave:d3.t1]
include/diff_tables.inc [master:d2.t1, slave:d2.t1]
include/diff_tables.inc [master:d1.t1, slave:d1.t1]
[connection master]
DROP DATABASE d1;
DROP DATABASE d2;
DROP DATABASE d3;
DROP DATABASE d4;
include/sync_slave_sql_with_master.inc
include/rpl_end.inc
