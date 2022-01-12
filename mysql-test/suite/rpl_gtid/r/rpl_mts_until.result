include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE DATABASE db1;
CREATE DATABASE db2;
CREATE DATABASE db3;
CREATE TABLE db1.t1(c1 INT PRIMARY KEY, c2 CHAR(20));
CREATE TABLE db2.t1(c1 INT PRIMARY KEY, c2 CHAR(20));
CREATE TABLE db3.t1(c1 INT PRIMARY KEY, c2 CHAR(20));
CREATE VIEW db1.v1 AS SELECT * FROM db1.t1 WHERE c1 <= 4;
include/sync_slave_sql_with_master.inc
include/stop_slave_sql.inc
SET @saved_replica_parallel_type = @@GLOBAL.replica_parallel_type;
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
SET @saved_replica_parallel_workers = @@GLOBAL.replica_parallel_workers;
SET @saved_replica_preserve_commit_order = @@GLOBAL.replica_preserve_commit_order;
SET @saved_innodb_lock_wait_timeout = @@GLOBAL.innodb_lock_wait_timeout;
SET @saved_relay_log_purge = @@GLOBAL.relay_log_purge;
SET GLOBAL innodb_lock_wait_timeout = 1000;
SET GLOBAL relay_log_purge = FALSE;
SET GLOBAL replica_parallel_type = "LOGICAL_CLOCK";
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
SET GLOBAL replica_parallel_workers = 4;
SET GLOBAL replica_preserve_commit_order=0;
[connection slave]
include/stop_slave_sql.inc
Warnings:
Note	3084	Replication thread(s) for channel '' are already stopped.
[connection master]
SET debug="d,set_commit_parent_100";
INSERT INTO db1.t1 VALUES(1, 'SOURCE_POS');
INSERT INTO db1.t1 VALUES(2, 'SOURCE_POS');
INSERT INTO db2.t1 VALUES(1, 'SOURCE_POS');
INSERT INTO db2.t1 VALUES(2, 'SOURCE_POS');
INSERT INTO db3.t1 VALUES(1, 'SOURCE_POS');
INSERT INTO db3.t1 VALUES(2, 'SOURCE_POS');
FLUSH BINARY LOGS;
INSERT INTO db1.t1 VALUES(3, 'SOURCE_POS');
INSERT INTO db1.t1 VALUES(4, 'SOURCE_POS');
INSERT INTO db2.t1 VALUES(3, 'SOURCE_POS');
INSERT INTO db2.t1 VALUES(4, 'SOURCE_POS');
INSERT INTO db3.t1 VALUES(3, 'SOURCE_POS');
INSERT INTO db3.t1 VALUES(4, 'SOURCE_POS');
INSERT INTO db1.t1 VALUES(5, 'SOURCE_POS');
INSERT INTO db1.t1 VALUES(6, 'SOURCE_POS');
INSERT INTO db2.t1 VALUES(5, 'SOURCE_POS');
INSERT INTO db2.t1 VALUES(6, 'SOURCE_POS');
INSERT INTO db3.t1 VALUES(5, 'SOURCE_POS');
INSERT INTO db3.t1 VALUES(6, 'SOURCE_POS');
include/save_master_pos.inc
[connection server_2]
BEGIN;
INSERT INTO db1.t1 VALUES(4, "block a worker");
[connection slave]
START SLAVE SQL_THREAD UNTIL SOURCE_LOG_FILE="SOURCE_LOG_FILE", SOURCE_LOG_POS=SOURCE_LOG_POS;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
[connection server_2]
ROLLBACK;
include/wait_for_slave_sql_to_stop.inc
include/diff_tables.inc [master:db1.v1, slave:db1.t1, slave:db2.t1, slave:db3.t1]
START SLAVE SQL_THREAD UNTIL SAME_UNTIO_OPTION;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_sql_to_stop.inc
include/diff_tables.inc [master:db1.v1, slave:db1.t1, slave:db2.t1, slave:db3.t1]
include/start_slave_sql.inc
[connection master]
TRUNCATE db1.t1;
TRUNCATE db2.t1;
TRUNCATE db3.t1;
include/sync_slave_sql_with_master.inc
[connection slave]
include/stop_slave_sql.inc
[connection master]
SET debug="d,set_commit_parent_100";
INSERT INTO db1.t1 VALUES(1, 'RELAY_POS');
INSERT INTO db1.t1 VALUES(2, 'RELAY_POS');
INSERT INTO db2.t1 VALUES(1, 'RELAY_POS');
INSERT INTO db2.t1 VALUES(2, 'RELAY_POS');
INSERT INTO db3.t1 VALUES(1, 'RELAY_POS');
INSERT INTO db3.t1 VALUES(2, 'RELAY_POS');
FLUSH BINARY LOGS;
INSERT INTO db1.t1 VALUES(3, 'RELAY_POS');
INSERT INTO db1.t1 VALUES(4, 'RELAY_POS');
INSERT INTO db2.t1 VALUES(3, 'RELAY_POS');
INSERT INTO db2.t1 VALUES(4, 'RELAY_POS');
INSERT INTO db3.t1 VALUES(3, 'RELAY_POS');
INSERT INTO db3.t1 VALUES(4, 'RELAY_POS');
include/sync_slave_io_with_master.inc
include/include/rpl_get_end_of_relay_log.inc
[connection master]
INSERT INTO db1.t1 VALUES(5, 'RELAY_POS');
INSERT INTO db1.t1 VALUES(6, 'RELAY_POS');
INSERT INTO db2.t1 VALUES(5, 'RELAY_POS');
INSERT INTO db2.t1 VALUES(6, 'RELAY_POS');
INSERT INTO db3.t1 VALUES(5, 'RELAY_POS');
INSERT INTO db3.t1 VALUES(6, 'RELAY_POS');
include/save_master_pos.inc
[connection server_2]
BEGIN;
INSERT INTO db1.t1 VALUES(4, "block a worker");
[connection slave]
START SLAVE SQL_THREAD UNTIL RELAY_LOG_FILE="RELAY_LOG_FILE", RELAY_LOG_POS=RELAY_LOG_POS;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
[connection server_2]
ROLLBACK;
include/wait_for_slave_sql_to_stop.inc
include/diff_tables.inc [master:db1.v1, slave:db1.t1, slave:db2.t1, slave:db3.t1]
START SLAVE SQL_THREAD UNTIL SAME_UNTIO_OPTION;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_sql_to_stop.inc
include/diff_tables.inc [master:db1.v1, slave:db1.t1, slave:db2.t1, slave:db3.t1]
include/start_slave_sql.inc
[connection master]
TRUNCATE db1.t1;
TRUNCATE db2.t1;
TRUNCATE db3.t1;
include/sync_slave_sql_with_master.inc
[connection slave]
include/stop_slave_sql.inc
[connection master]
SET debug="d,set_commit_parent_100";
INSERT INTO db1.t1 VALUES(1, 'BEFORE_GTIDS');
INSERT INTO db1.t1 VALUES(2, 'BEFORE_GTIDS');
INSERT INTO db2.t1 VALUES(1, 'BEFORE_GTIDS');
INSERT INTO db2.t1 VALUES(2, 'BEFORE_GTIDS');
INSERT INTO db3.t1 VALUES(1, 'BEFORE_GTIDS');
INSERT INTO db3.t1 VALUES(2, 'BEFORE_GTIDS');
FLUSH BINARY LOGS;
INSERT INTO db1.t1 VALUES(3, 'BEFORE_GTIDS');
INSERT INTO db1.t1 VALUES(4, 'BEFORE_GTIDS');
INSERT INTO db2.t1 VALUES(3, 'BEFORE_GTIDS');
INSERT INTO db2.t1 VALUES(4, 'BEFORE_GTIDS');
INSERT INTO db3.t1 VALUES(3, 'BEFORE_GTIDS');
INSERT INTO db3.t1 VALUES(4, 'BEFORE_GTIDS');
SET @gtids_should_applied = @@GLOBAL.gtid_executed;
INSERT INTO db1.t1 VALUES(5, 'BEFORE_GTIDS');
INSERT INTO db1.t1 VALUES(6, 'BEFORE_GTIDS');
INSERT INTO db2.t1 VALUES(5, 'BEFORE_GTIDS');
INSERT INTO db2.t1 VALUES(6, 'BEFORE_GTIDS');
INSERT INTO db3.t1 VALUES(5, 'BEFORE_GTIDS');
INSERT INTO db3.t1 VALUES(6, 'BEFORE_GTIDS');
include/save_master_pos.inc
[connection server_2]
BEGIN;
INSERT INTO db1.t1 VALUES(4, "block a worker");
[connection slave]
START SLAVE SQL_THREAD UNTIL SQL_BEFORE_GTIDS="GTIDS";
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
[connection server_2]
ROLLBACK;
include/wait_for_slave_sql_to_stop.inc
include/diff_tables.inc [master:db1.v1, slave:db1.t1, slave:db2.t1, slave:db3.t1]
START SLAVE SQL_THREAD UNTIL SAME_UNTIO_OPTION;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_sql_to_stop.inc
include/diff_tables.inc [master:db1.v1, slave:db1.t1, slave:db2.t1, slave:db3.t1]
include/start_slave_sql.inc
[connection master]
TRUNCATE db1.t1;
TRUNCATE db2.t1;
TRUNCATE db3.t1;
include/sync_slave_sql_with_master.inc
include/stop_slave_sql.inc
SET GLOBAL replica_parallel_type = "DATABASE";
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
SET GLOBAL replica_parallel_workers = 4;
SET GLOBAL innodb_lock_wait_timeout = 1000;
SET GLOBAL relay_log_purge = FALSE;
[connection slave]
include/stop_slave_sql.inc
Warnings:
Note	3084	Replication thread(s) for channel '' are already stopped.
[connection master]
SET debug="d,set_commit_parent_100";
INSERT INTO db1.t1 VALUES(1, 'SOURCE_POS');
INSERT INTO db1.t1 VALUES(2, 'SOURCE_POS');
INSERT INTO db2.t1 VALUES(1, 'SOURCE_POS');
INSERT INTO db2.t1 VALUES(2, 'SOURCE_POS');
INSERT INTO db3.t1 VALUES(1, 'SOURCE_POS');
INSERT INTO db3.t1 VALUES(2, 'SOURCE_POS');
FLUSH BINARY LOGS;
INSERT INTO db1.t1 VALUES(3, 'SOURCE_POS');
INSERT INTO db1.t1 VALUES(4, 'SOURCE_POS');
INSERT INTO db2.t1 VALUES(3, 'SOURCE_POS');
INSERT INTO db2.t1 VALUES(4, 'SOURCE_POS');
INSERT INTO db3.t1 VALUES(3, 'SOURCE_POS');
INSERT INTO db3.t1 VALUES(4, 'SOURCE_POS');
INSERT INTO db1.t1 VALUES(5, 'SOURCE_POS');
INSERT INTO db1.t1 VALUES(6, 'SOURCE_POS');
INSERT INTO db2.t1 VALUES(5, 'SOURCE_POS');
INSERT INTO db2.t1 VALUES(6, 'SOURCE_POS');
INSERT INTO db3.t1 VALUES(5, 'SOURCE_POS');
INSERT INTO db3.t1 VALUES(6, 'SOURCE_POS');
include/save_master_pos.inc
[connection server_2]
BEGIN;
INSERT INTO db1.t1 VALUES(4, "block a worker");
[connection slave]
START SLAVE SQL_THREAD UNTIL SOURCE_LOG_FILE="SOURCE_LOG_FILE", SOURCE_LOG_POS=SOURCE_LOG_POS;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
[connection server_2]
ROLLBACK;
include/wait_for_slave_sql_to_stop.inc
include/diff_tables.inc [master:db1.v1, slave:db1.t1, slave:db2.t1, slave:db3.t1]
START SLAVE SQL_THREAD UNTIL SAME_UNTIO_OPTION;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_sql_to_stop.inc
include/diff_tables.inc [master:db1.v1, slave:db1.t1, slave:db2.t1, slave:db3.t1]
include/start_slave_sql.inc
[connection master]
TRUNCATE db1.t1;
TRUNCATE db2.t1;
TRUNCATE db3.t1;
include/sync_slave_sql_with_master.inc
[connection slave]
include/stop_slave_sql.inc
[connection master]
SET debug="d,set_commit_parent_100";
INSERT INTO db1.t1 VALUES(1, 'RELAY_POS');
INSERT INTO db1.t1 VALUES(2, 'RELAY_POS');
INSERT INTO db2.t1 VALUES(1, 'RELAY_POS');
INSERT INTO db2.t1 VALUES(2, 'RELAY_POS');
INSERT INTO db3.t1 VALUES(1, 'RELAY_POS');
INSERT INTO db3.t1 VALUES(2, 'RELAY_POS');
FLUSH BINARY LOGS;
INSERT INTO db1.t1 VALUES(3, 'RELAY_POS');
INSERT INTO db1.t1 VALUES(4, 'RELAY_POS');
INSERT INTO db2.t1 VALUES(3, 'RELAY_POS');
INSERT INTO db2.t1 VALUES(4, 'RELAY_POS');
INSERT INTO db3.t1 VALUES(3, 'RELAY_POS');
INSERT INTO db3.t1 VALUES(4, 'RELAY_POS');
include/sync_slave_io_with_master.inc
include/include/rpl_get_end_of_relay_log.inc
[connection master]
INSERT INTO db1.t1 VALUES(5, 'RELAY_POS');
INSERT INTO db1.t1 VALUES(6, 'RELAY_POS');
INSERT INTO db2.t1 VALUES(5, 'RELAY_POS');
INSERT INTO db2.t1 VALUES(6, 'RELAY_POS');
INSERT INTO db3.t1 VALUES(5, 'RELAY_POS');
INSERT INTO db3.t1 VALUES(6, 'RELAY_POS');
include/save_master_pos.inc
[connection server_2]
BEGIN;
INSERT INTO db1.t1 VALUES(4, "block a worker");
[connection slave]
START SLAVE SQL_THREAD UNTIL RELAY_LOG_FILE="RELAY_LOG_FILE", RELAY_LOG_POS=RELAY_LOG_POS;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
[connection server_2]
ROLLBACK;
include/wait_for_slave_sql_to_stop.inc
include/diff_tables.inc [master:db1.v1, slave:db1.t1, slave:db2.t1, slave:db3.t1]
START SLAVE SQL_THREAD UNTIL SAME_UNTIO_OPTION;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_sql_to_stop.inc
include/diff_tables.inc [master:db1.v1, slave:db1.t1, slave:db2.t1, slave:db3.t1]
include/start_slave_sql.inc
[connection master]
TRUNCATE db1.t1;
TRUNCATE db2.t1;
TRUNCATE db3.t1;
include/sync_slave_sql_with_master.inc
[connection slave]
include/stop_slave_sql.inc
[connection master]
SET debug="d,set_commit_parent_100";
INSERT INTO db1.t1 VALUES(1, 'BEFORE_GTIDS');
INSERT INTO db1.t1 VALUES(2, 'BEFORE_GTIDS');
INSERT INTO db2.t1 VALUES(1, 'BEFORE_GTIDS');
INSERT INTO db2.t1 VALUES(2, 'BEFORE_GTIDS');
INSERT INTO db3.t1 VALUES(1, 'BEFORE_GTIDS');
INSERT INTO db3.t1 VALUES(2, 'BEFORE_GTIDS');
FLUSH BINARY LOGS;
INSERT INTO db1.t1 VALUES(3, 'BEFORE_GTIDS');
INSERT INTO db1.t1 VALUES(4, 'BEFORE_GTIDS');
INSERT INTO db2.t1 VALUES(3, 'BEFORE_GTIDS');
INSERT INTO db2.t1 VALUES(4, 'BEFORE_GTIDS');
INSERT INTO db3.t1 VALUES(3, 'BEFORE_GTIDS');
INSERT INTO db3.t1 VALUES(4, 'BEFORE_GTIDS');
SET @gtids_should_applied = @@GLOBAL.gtid_executed;
INSERT INTO db1.t1 VALUES(5, 'BEFORE_GTIDS');
INSERT INTO db1.t1 VALUES(6, 'BEFORE_GTIDS');
INSERT INTO db2.t1 VALUES(5, 'BEFORE_GTIDS');
INSERT INTO db2.t1 VALUES(6, 'BEFORE_GTIDS');
INSERT INTO db3.t1 VALUES(5, 'BEFORE_GTIDS');
INSERT INTO db3.t1 VALUES(6, 'BEFORE_GTIDS');
include/save_master_pos.inc
[connection server_2]
BEGIN;
INSERT INTO db1.t1 VALUES(4, "block a worker");
[connection slave]
START SLAVE SQL_THREAD UNTIL SQL_BEFORE_GTIDS="GTIDS";
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
[connection server_2]
ROLLBACK;
include/wait_for_slave_sql_to_stop.inc
include/diff_tables.inc [master:db1.v1, slave:db1.t1, slave:db2.t1, slave:db3.t1]
START SLAVE SQL_THREAD UNTIL SAME_UNTIO_OPTION;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_sql_to_stop.inc
include/diff_tables.inc [master:db1.v1, slave:db1.t1, slave:db2.t1, slave:db3.t1]
include/start_slave_sql.inc
[connection master]
TRUNCATE db1.t1;
TRUNCATE db2.t1;
TRUNCATE db3.t1;
include/sync_slave_sql_with_master.inc
[connection master]
DROP DATABASE db1;
DROP DATABASE db2;
DROP DATABASE db3;
include/sync_slave_sql_with_master.inc
include/stop_slave_sql.inc
SET GLOBAL replica_parallel_type = @saved_replica_parallel_type;
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
SET GLOBAL replica_parallel_workers = @saved_replica_parallel_workers;
SET GLOBAL replica_preserve_commit_order= @saved_replica_preserve_commit_order;
SET GLOBAL innodb_lock_wait_timeout = @saved_innodb_lock_wait_timeout;
SET GLOBAL relay_log_purge = @saved_relay_log_purge;
include/rpl_end.inc
