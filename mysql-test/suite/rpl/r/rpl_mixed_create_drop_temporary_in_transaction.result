include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
#
# Verify that CREATE TEMPORARY TABLE in a single STMT transaction
# is not written into binlog after committing the transaction.
#
BEGIN;
CREATE TEMPORARY TABLE temp1 (i INT);
COMMIT;
include/show_binlog_events.inc
include/sync_slave_sql_with_master.inc
include/assert.inc [Replica_open_temp_tables shall be 0]
[connection master]
#
# Verify that DROP TEMPORARY TABLE in a single STMT transaction
# is not written into binlog after committing the transaction.
#
BEGIN;
DROP TEMPORARY TABLE temp1;
COMMIT;
include/show_binlog_events.inc
CREATE TABLE t1 (c1 INT);
#
# Verify that CREATE or DML against TEMPORARY TABLE in a MULTI STMTs
# transaction is not written into binlog after committing the
# transaction.
#
BEGIN;
INSERT INTO t1 VALUES (1);
CREATE TEMPORARY TABLE temp1 (i INT);
INSERT INTO temp1 VALUES (1);
INSERT INTO t1 VALUES (2);
COMMIT;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (1)
master-bin.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (2)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
include/sync_slave_sql_with_master.inc
include/assert.inc [Replica_open_temp_tables shall be 0]
[connection master]
#
# Verify that DROP TEMPORARY TABLE in a MULTI STMTs transaction
# is not written into binlog after committing the transaction.
#
BEGIN;
INSERT INTO t1 VALUES (3);
INSERT INTO temp1 VALUES (2);
DROP TEMPORARY TABLE temp1;
INSERT INTO t1 VALUES (4);
COMMIT;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (3)
master-bin.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (4)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
include/sync_slave_sql_with_master.inc
include/assert.inc [Replica_open_temp_tables shall be 0]
[connection master]
#
# Verify that CREATE TEMPORARY TABLE in a single STMT transaction
# is not written into binlog after rolling back the transaction.
#
BEGIN;
CREATE TEMPORARY TABLE temp1 (i INT);
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
include/show_binlog_events.inc
include/sync_slave_sql_with_master.inc
include/assert.inc [Replica_open_temp_tables shall be 0]
[connection master]
#
# Verify that DROP TEMPORARY TABLE in a single STMT transaction
# is not written into binlog after rolling back the transaction.
#
BEGIN;
DROP TEMPORARY TABLE temp1;
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
include/show_binlog_events.inc
#
# Verify that CREATE or DML against TEMPORARY TABLE in a MULTI STMTs
# transaction is not written into binlog after rolling back the
# transaction.
#
BEGIN;
INSERT INTO t1 VALUES (5);
CREATE TEMPORARY TABLE temp1 (i INT);
INSERT INTO temp1 VALUES (1);
INSERT INTO t1 VALUES (6);
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
include/show_binlog_events.inc
include/sync_slave_sql_with_master.inc
include/assert.inc [Replica_open_temp_tables shall be 0]
[connection master]
#
# Verify that DROP TEMPORARY TABLE in a MULTI STMTs transaction
# is not written into binlog after rolling back the transaction.
#
BEGIN;
INSERT INTO t1 VALUES (7);
INSERT INTO temp1 VALUES (2);
DROP TEMPORARY TABLE temp1;
INSERT INTO t1 VALUES (8);
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
include/show_binlog_events.inc
include/sync_slave_sql_with_master.inc
include/assert.inc [Replica_open_temp_tables shall be 0]
[connection master]
CREATE PROCEDURE sp1 ()
BEGIN
CREATE TEMPORARY TABLE temp1 (i INT);
END//
CREATE PROCEDURE sp2 ()
BEGIN
DROP TEMPORARY TABLE temp1;
END//
CREATE PROCEDURE sp3 ()
BEGIN
INSERT INTO t1 VALUES (9);
CREATE TEMPORARY TABLE temp1 (i INT);
INSERT INTO temp1 VALUES (1);
END//
CREATE PROCEDURE sp4 ()
BEGIN
INSERT INTO t1 VALUES (10);
INSERT INTO temp1 VALUES (2);
DROP TEMPORARY TABLE temp1;
END//
#
# Verify that CREATE TEMPORARY TABLE in a single STMT procedure
# is not written into binlog after committing the transaction.
#
START TRANSACTION;
CALL sp1();
COMMIT;
include/show_binlog_events.inc
include/sync_slave_sql_with_master.inc
include/assert.inc [Replica_open_temp_tables shall be 0]
[connection master]
#
# Verify that DROP TEMPORARY TABLE in a single STMT procedure
# is not written into binlog after committing the transaction.
#
START TRANSACTION;
CALL sp2();
COMMIT;
include/show_binlog_events.inc
#
# Verify that CREATE or DML against TEMPORARY TABLE in a PROCEDURE
# is not written into binlog after committing the transaction.
#
START TRANSACTION;
CALL sp3();
COMMIT;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (9)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
include/sync_slave_sql_with_master.inc
include/assert.inc [Replica_open_temp_tables shall be 0]
[connection master]
#
# Verify that DROP or DML against TEMPORARY TABLE in a PROCEDURE
# is not written into binlog after committing the transaction.
#
START TRANSACTION;
CALL sp4();
COMMIT;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (10)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
include/sync_slave_sql_with_master.inc
include/assert.inc [Replica_open_temp_tables shall be 0]
[connection master]
#
# Verify that CREATE TEMPORARY TABLE in a single STMT procedure
# is not written into binlog after rolling back the transaction.
#
START TRANSACTION;
CALL sp1();
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
include/show_binlog_events.inc
include/sync_slave_sql_with_master.inc
include/assert.inc [Replica_open_temp_tables shall be 0]
[connection master]
#
# Verify that DROP TEMPORARY TABLE in a single STMT procedure
# is not written into binlog after rolling back the transaction.
#
START TRANSACTION;
CALL sp2();
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
include/show_binlog_events.inc
#
# Verify that CREATE or DML against TEMPORARY TABLE in a PROCEDURE
# is not written into binlog after rolling back the transaction.
#
START TRANSACTION;
CALL sp3();
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
include/show_binlog_events.inc
include/sync_slave_sql_with_master.inc
include/assert.inc [Replica_open_temp_tables shall be 0]
[connection master]
#
# Verify that DROP or DML against TEMPORARY TABLE in a PROCEDURE
# is not written into binlog after rolling back the transaction.
#
START TRANSACTION;
CALL sp4();
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
include/show_binlog_events.inc
include/sync_slave_sql_with_master.inc
include/assert.inc [Replica_open_temp_tables shall be 0]
[connection master]
DROP TABLE t1;
DROP PROCEDURE sp1;
DROP PROCEDURE sp2;
DROP PROCEDURE sp3;
DROP PROCEDURE sp4;
include/rpl_end.inc
