include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection master]
SET @@session.binlog_transaction_compression = ON;
[connection slave]
include/stop_slave_sql.inc
SET @@global.replica_transaction_retries= 200;
SET @@global.innodb_lock_wait_timeout= 5;
SET @@global.binlog_transaction_compression = ON;
SET @@global.replica_parallel_workers = 4;
SET @@global.replica_preserve_commit_order = OFF;
include/start_slave_sql.inc
include/stop_slave_sql.inc
SET @@global.replica_parallel_workers = 8;
include/start_slave_sql.inc
[connection master]
include/rpl_reset.inc
CREATE TABLE t1 (a INT PRIMARY KEY, b INT);
include/sync_slave_sql_with_master.inc
include/stop_slave_sql.inc
SET @@global.replica_parallel_type = 'DATABASE';
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
include/start_slave_sql.inc
[connection slave1]
BEGIN;
INSERT INTO t1 SET a = 10, b = 11;
[connection master]
INSERT INTO t1 SET a = 22, b = 22;
XA START 'xa1';
INSERT INTO t1 SET a = 10, b = 1;
XA END 'xa1';
XA PREPARE 'xa1';
include/assert_binlog_events.inc [Gtid # Query # Gtid # Transaction_payload # Query # Table_map # Write_rows # Xid # Gtid # Transaction_payload # Query/XA START.* # Table_map # Write_rows # Query/XA END.* # XA_prepare]
[connection slave]
[connection slave1]
ROLLBACK;
[connection master]
XA COMMIT 'xa1';
include/sync_slave_sql_with_master.inc
include/assert_binlog_events.inc [Gtid # Query # Gtid # Transaction_payload # Query # Table_map # Write_rows # Xid # Gtid # Transaction_payload # Query/XA START.* # Table_map # Write_rows # Query/XA END.* # XA_prepare # Gtid # Query/XA COMMIT.*]
include/diff_tables.inc [master:test.t1,slave:test.t1]
[connection master]
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
[connection master]
include/rpl_reset.inc
CREATE TABLE t1 (a INT PRIMARY KEY, b INT);
include/sync_slave_sql_with_master.inc
include/stop_slave_sql.inc
SET @@global.replica_parallel_type = 'LOGICAL_CLOCK';
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
include/start_slave_sql.inc
[connection slave1]
BEGIN;
INSERT INTO t1 SET a = 10, b = 11;
[connection master]
INSERT INTO t1 SET a = 22, b = 22;
XA START 'xa1';
INSERT INTO t1 SET a = 10, b = 1;
XA END 'xa1';
XA PREPARE 'xa1';
include/assert_binlog_events.inc [Gtid # Query # Gtid # Transaction_payload # Query # Table_map # Write_rows # Xid # Gtid # Transaction_payload # Query/XA START.* # Table_map # Write_rows # Query/XA END.* # XA_prepare]
[connection slave]
[connection slave1]
ROLLBACK;
[connection master]
XA COMMIT 'xa1';
include/sync_slave_sql_with_master.inc
include/assert_binlog_events.inc [Gtid # Query # Gtid # Transaction_payload # Query # Table_map # Write_rows # Xid # Gtid # Transaction_payload # Query/XA START.* # Table_map # Write_rows # Query/XA END.* # XA_prepare # Gtid # Query/XA COMMIT.*]
include/diff_tables.inc [master:test.t1,slave:test.t1]
[connection master]
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
include/stop_slave_sql.inc
SET @@global.replica_parallel_workers = 1;
include/start_slave_sql.inc
[connection master]
include/rpl_reset.inc
CREATE TABLE t1 (a INT PRIMARY KEY, b INT);
include/sync_slave_sql_with_master.inc
include/stop_slave_sql.inc
SET @@global.replica_parallel_type = 'DATABASE';
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
include/start_slave_sql.inc
[connection slave1]
BEGIN;
INSERT INTO t1 SET a = 10, b = 11;
[connection master]
INSERT INTO t1 SET a = 22, b = 22;
XA START 'xa1';
INSERT INTO t1 SET a = 10, b = 1;
XA END 'xa1';
XA PREPARE 'xa1';
include/assert_binlog_events.inc [Gtid # Query # Gtid # Transaction_payload # Query # Table_map # Write_rows # Xid # Gtid # Transaction_payload # Query/XA START.* # Table_map # Write_rows # Query/XA END.* # XA_prepare]
[connection slave]
[connection slave1]
ROLLBACK;
[connection master]
XA COMMIT 'xa1';
include/sync_slave_sql_with_master.inc
include/assert_binlog_events.inc [Gtid # Query # Gtid # Transaction_payload # Query # Table_map # Write_rows # Xid # Gtid # Transaction_payload # Query/XA START.* # Table_map # Write_rows # Query/XA END.* # XA_prepare # Gtid # Query/XA COMMIT.*]
include/diff_tables.inc [master:test.t1,slave:test.t1]
[connection master]
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
[connection master]
include/rpl_reset.inc
CREATE TABLE t1 (a INT PRIMARY KEY, b INT);
include/sync_slave_sql_with_master.inc
include/stop_slave_sql.inc
SET @@global.replica_parallel_type = 'LOGICAL_CLOCK';
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
include/start_slave_sql.inc
[connection slave1]
BEGIN;
INSERT INTO t1 SET a = 10, b = 11;
[connection master]
INSERT INTO t1 SET a = 22, b = 22;
XA START 'xa1';
INSERT INTO t1 SET a = 10, b = 1;
XA END 'xa1';
XA PREPARE 'xa1';
include/assert_binlog_events.inc [Gtid # Query # Gtid # Transaction_payload # Query # Table_map # Write_rows # Xid # Gtid # Transaction_payload # Query/XA START.* # Table_map # Write_rows # Query/XA END.* # XA_prepare]
[connection slave]
[connection slave1]
ROLLBACK;
[connection master]
XA COMMIT 'xa1';
include/sync_slave_sql_with_master.inc
include/assert_binlog_events.inc [Gtid # Query # Gtid # Transaction_payload # Query # Table_map # Write_rows # Xid # Gtid # Transaction_payload # Query/XA START.* # Table_map # Write_rows # Query/XA END.* # XA_prepare # Gtid # Query/XA COMMIT.*]
include/diff_tables.inc [master:test.t1,slave:test.t1]
[connection master]
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
include/stop_slave_sql.inc
SET @@global.replica_parallel_workers = 0;
Warnings:
Warning	1287	'0' is deprecated and will be removed in a future release. Please use 1 instead
include/start_slave_sql.inc
[connection master]
include/rpl_reset.inc
CREATE TABLE t1 (a INT PRIMARY KEY, b INT);
include/sync_slave_sql_with_master.inc
include/stop_slave_sql.inc
SET @@global.replica_parallel_type = 'DATABASE';
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
include/start_slave_sql.inc
[connection slave1]
BEGIN;
INSERT INTO t1 SET a = 10, b = 11;
[connection master]
INSERT INTO t1 SET a = 22, b = 22;
XA START 'xa1';
INSERT INTO t1 SET a = 10, b = 1;
XA END 'xa1';
XA PREPARE 'xa1';
include/assert_binlog_events.inc [Gtid # Query # Gtid # Transaction_payload # Query # Table_map # Write_rows # Xid # Gtid # Transaction_payload # Query/XA START.* # Table_map # Write_rows # Query/XA END.* # XA_prepare]
[connection slave]
[connection slave1]
ROLLBACK;
[connection master]
XA COMMIT 'xa1';
include/sync_slave_sql_with_master.inc
include/assert_binlog_events.inc [Gtid # Query # Gtid # Transaction_payload # Query # Table_map # Write_rows # Xid # Gtid # Transaction_payload # Query/XA START.* # Table_map # Write_rows # Query/XA END.* # XA_prepare # Gtid # Query/XA COMMIT.*]
include/diff_tables.inc [master:test.t1,slave:test.t1]
[connection master]
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
[connection master]
include/rpl_reset.inc
CREATE TABLE t1 (a INT PRIMARY KEY, b INT);
include/sync_slave_sql_with_master.inc
include/stop_slave_sql.inc
SET @@global.replica_parallel_type = 'LOGICAL_CLOCK';
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
include/start_slave_sql.inc
[connection slave1]
BEGIN;
INSERT INTO t1 SET a = 10, b = 11;
[connection master]
INSERT INTO t1 SET a = 22, b = 22;
XA START 'xa1';
INSERT INTO t1 SET a = 10, b = 1;
XA END 'xa1';
XA PREPARE 'xa1';
include/assert_binlog_events.inc [Gtid # Query # Gtid # Transaction_payload # Query # Table_map # Write_rows # Xid # Gtid # Transaction_payload # Query/XA START.* # Table_map # Write_rows # Query/XA END.* # XA_prepare]
[connection slave]
[connection slave1]
ROLLBACK;
[connection master]
XA COMMIT 'xa1';
include/sync_slave_sql_with_master.inc
include/assert_binlog_events.inc [Gtid # Query # Gtid # Transaction_payload # Query # Table_map # Write_rows # Xid # Gtid # Transaction_payload # Query/XA START.* # Table_map # Write_rows # Query/XA END.* # XA_prepare # Gtid # Query/XA COMMIT.*]
include/diff_tables.inc [master:test.t1,slave:test.t1]
[connection master]
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
SET @@global.replica_transaction_retries= SAVED;
SET @@global.innodb_lock_wait_timeout= SAVED;
SET @@global.binlog_transaction_compression = SAVED;
SET @@global.replica_parallel_workers = SAVED;
SET @@global.replica_parallel_type = 'SAVED';
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
SET @@global.replica_preserve_commit_order = SAVED;
include/start_slave.inc
[connection master]
SET @@session.binlog_transaction_compression = SAVED;
include/rpl_end.inc
