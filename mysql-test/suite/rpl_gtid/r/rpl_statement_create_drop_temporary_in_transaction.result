include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
#
# Verify that CREATE TEMPORARY TABLE in a single STMT transaction
# is disallowed and not written into binlog after committing the
# transaction.
#
BEGIN;
CREATE TEMPORARY TABLE temp1 (i INT);
ERROR HY000: Statement violates GTID consistency: CREATE TEMPORARY TABLE and DROP TEMPORARY TABLE are not allowed inside a transaction or inside a procedure in a transactional context when @@session.binlog_format=STATEMENT.
COMMIT;
include/show_binlog_events.inc
include/sync_slave_sql_with_master.inc
include/assert.inc [Replica_open_temp_tables shall be 0]
[connection master]
#
# Verify that DROP TEMPORARY TABLE in a single STMT transaction
# is disallowed and not written into binlog after committing
# the transaction.
#
BEGIN;
DROP TEMPORARY TABLE temp1;
ERROR HY000: Statement violates GTID consistency: CREATE TEMPORARY TABLE and DROP TEMPORARY TABLE are not allowed inside a transaction or inside a procedure in a transactional context when @@session.binlog_format=STATEMENT.
COMMIT;
include/show_binlog_events.inc
CREATE TABLE t1 (c1 INT);
#
# Verify that CREATE TEMPORARY TABLE in a MULTI STMTs transaction
# is disallowed and not written into binlog after committing the
# transaction.
#
BEGIN;
INSERT INTO t1 VALUES (1);
CREATE TEMPORARY TABLE temp1 (i INT);
ERROR HY000: Statement violates GTID consistency: CREATE TEMPORARY TABLE and DROP TEMPORARY TABLE are not allowed inside a transaction or inside a procedure in a transactional context when @@session.binlog_format=STATEMENT.
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
# is disallowed and not written into binlog after committing
# the transaction.
#
BEGIN;
INSERT INTO t1 VALUES (3);
DROP TEMPORARY TABLE temp1;
ERROR HY000: Statement violates GTID consistency: CREATE TEMPORARY TABLE and DROP TEMPORARY TABLE are not allowed inside a transaction or inside a procedure in a transactional context when @@session.binlog_format=STATEMENT.
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
END//
CREATE PROCEDURE sp4 ()
BEGIN
INSERT INTO t1 VALUES (10);
DROP TEMPORARY TABLE temp1;
END//
#
# Verify that CREATE TEMPORARY TABLE in a single STMT procedure
# in transactional context is disallowed and not written into
# binlog after committing the transaction.
#
START TRANSACTION;
CALL sp1();
ERROR HY000: Statement violates GTID consistency: CREATE TEMPORARY TABLE and DROP TEMPORARY TABLE are not allowed inside a transaction or inside a procedure in a transactional context when @@session.binlog_format=STATEMENT.
COMMIT;
include/show_binlog_events.inc
include/sync_slave_sql_with_master.inc
include/assert.inc [Replica_open_temp_tables shall be 0]
[connection master]
#
# Verify that DROP TEMPORARY TABLE in a single STMT procedure
# in transactional context is disallowed and not written into
# binlog after committing the transaction.
#
START TRANSACTION;
CALL sp2();
ERROR HY000: Statement violates GTID consistency: CREATE TEMPORARY TABLE and DROP TEMPORARY TABLE are not allowed inside a transaction or inside a procedure in a transactional context when @@session.binlog_format=STATEMENT.
COMMIT;
include/show_binlog_events.inc
#
# Verify that CREATE TEMPORARY TABLE in a PROCEDURE in transactional
# context is disallowed and not written into binlog after committing
# the transaction.
#
START TRANSACTION;
CALL sp3();
ERROR HY000: Statement violates GTID consistency: CREATE TEMPORARY TABLE and DROP TEMPORARY TABLE are not allowed inside a transaction or inside a procedure in a transactional context when @@session.binlog_format=STATEMENT.
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
# Verify that DROP TEMPORARY TABLE in a PROCEDURE in transactional
# context is disallowed and not written into binlog after
# committing the transaction.
#
START TRANSACTION;
CALL sp4();
ERROR HY000: Statement violates GTID consistency: CREATE TEMPORARY TABLE and DROP TEMPORARY TABLE are not allowed inside a transaction or inside a procedure in a transactional context when @@session.binlog_format=STATEMENT.
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
# Verify that CREATE/DROP TEMPORARY TABLE in a procedure outside
# transactional context is allowed and written into binlog after
# calling the procedure.
#
CALL sp1();
CALL sp2();
CALL sp3();
CALL sp4();
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (10)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; CREATE TEMPORARY TABLE temp1 (i INT)
master-bin.000001	#	Query	#	#	use `test`; DROP TEMPORARY TABLE `temp1` /* generated by server */
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (9)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; CREATE TEMPORARY TABLE temp1 (i INT)
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (10)
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; DROP TEMPORARY TABLE `temp1` /* generated by server */
DROP TABLE t1;
DROP PROCEDURE sp1;
DROP PROCEDURE sp2;
DROP PROCEDURE sp3;
DROP PROCEDURE sp4;
include/rpl_end.inc
