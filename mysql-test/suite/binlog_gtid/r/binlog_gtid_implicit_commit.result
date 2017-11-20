CREATE TABLE t1 (id INT) ENGINE = InnoDB;
SET BINLOG_FORMAT = STATEMENT;
RESET MASTER;
SET AUTOCOMMIT = 0;
INSERT INTO t1 VALUES (1);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
SET AUTOCOMMIT = 1;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (1)
binlog.000001	#	Xid	#	#	COMMIT /* XID */
COMMIT;
RESET MASTER;
SET AUTOCOMMIT = 0;
BEGIN;
INSERT INTO t1 VALUES (2);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
SET AUTOCOMMIT = 1;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (2)
binlog.000001	#	Xid	#	#	COMMIT /* XID */
INSERT INTO t1 VALUES (3);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (2)
binlog.000001	#	Xid	#	#	COMMIT /* XID */
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (3)
binlog.000001	#	Xid	#	#	COMMIT /* XID */
COMMIT;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (2)
binlog.000001	#	Xid	#	#	COMMIT /* XID */
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (3)
binlog.000001	#	Xid	#	#	COMMIT /* XID */
COMMIT;
RESET MASTER;
SET AUTOCOMMIT = 1;
INSERT INTO t1 VALUES (1);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (1)
binlog.000001	#	Xid	#	#	COMMIT /* XID */
SET AUTOCOMMIT = 1;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (1)
binlog.000001	#	Xid	#	#	COMMIT /* XID */
COMMIT;
RESET MASTER;
SET AUTOCOMMIT = 1;
BEGIN;
INSERT INTO t1 VALUES (2);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
SET AUTOCOMMIT = 1;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
INSERT INTO t1 VALUES (3);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
COMMIT;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (2)
binlog.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (3)
binlog.000001	#	Xid	#	#	COMMIT /* XID */
COMMIT;
RESET MASTER;
SET AUTOCOMMIT = 0;
INSERT INTO t1 VALUES (1);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
SET AUTOCOMMIT = 0;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
COMMIT;
RESET MASTER;
SET AUTOCOMMIT = 0;
BEGIN;
INSERT INTO t1 VALUES (2);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
SET AUTOCOMMIT = 0;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
INSERT INTO t1 VALUES (3);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
COMMIT;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (2)
binlog.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (3)
binlog.000001	#	Xid	#	#	COMMIT /* XID */
COMMIT;
RESET MASTER;
SET AUTOCOMMIT = 1;
INSERT INTO t1 VALUES (1);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (1)
binlog.000001	#	Xid	#	#	COMMIT /* XID */
SET AUTOCOMMIT = 0;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (1)
binlog.000001	#	Xid	#	#	COMMIT /* XID */
COMMIT;
RESET MASTER;
SET AUTOCOMMIT = 1;
BEGIN;
INSERT INTO t1 VALUES (2);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
SET AUTOCOMMIT = 0;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
INSERT INTO t1 VALUES (3);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
COMMIT;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (2)
binlog.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (3)
binlog.000001	#	Xid	#	#	COMMIT /* XID */
COMMIT;
SET BINLOG_FORMAT = 'ROW';
RESET MASTER;
SET AUTOCOMMIT = 0;
INSERT INTO t1 VALUES (1);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
SET AUTOCOMMIT = 1;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Table_map	#	#	table_id: # (test.t1)
binlog.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
binlog.000001	#	Xid	#	#	COMMIT /* XID */
COMMIT;
RESET MASTER;
SET AUTOCOMMIT = 0;
BEGIN;
INSERT INTO t1 VALUES (2);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
SET AUTOCOMMIT = 1;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Table_map	#	#	table_id: # (test.t1)
binlog.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
binlog.000001	#	Xid	#	#	COMMIT /* XID */
INSERT INTO t1 VALUES (3);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Table_map	#	#	table_id: # (test.t1)
binlog.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
binlog.000001	#	Xid	#	#	COMMIT /* XID */
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Table_map	#	#	table_id: # (test.t1)
binlog.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
binlog.000001	#	Xid	#	#	COMMIT /* XID */
COMMIT;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Table_map	#	#	table_id: # (test.t1)
binlog.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
binlog.000001	#	Xid	#	#	COMMIT /* XID */
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Table_map	#	#	table_id: # (test.t1)
binlog.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
binlog.000001	#	Xid	#	#	COMMIT /* XID */
COMMIT;
RESET MASTER;
SET AUTOCOMMIT = 1;
INSERT INTO t1 VALUES (1);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Table_map	#	#	table_id: # (test.t1)
binlog.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
binlog.000001	#	Xid	#	#	COMMIT /* XID */
SET AUTOCOMMIT = 1;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Table_map	#	#	table_id: # (test.t1)
binlog.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
binlog.000001	#	Xid	#	#	COMMIT /* XID */
COMMIT;
RESET MASTER;
SET AUTOCOMMIT = 1;
BEGIN;
INSERT INTO t1 VALUES (2);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
SET AUTOCOMMIT = 1;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
INSERT INTO t1 VALUES (3);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
COMMIT;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Table_map	#	#	table_id: # (test.t1)
binlog.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
binlog.000001	#	Table_map	#	#	table_id: # (test.t1)
binlog.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
binlog.000001	#	Xid	#	#	COMMIT /* XID */
COMMIT;
RESET MASTER;
SET AUTOCOMMIT = 0;
INSERT INTO t1 VALUES (1);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
SET AUTOCOMMIT = 0;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
COMMIT;
RESET MASTER;
SET AUTOCOMMIT = 0;
BEGIN;
INSERT INTO t1 VALUES (2);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
SET AUTOCOMMIT = 0;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
INSERT INTO t1 VALUES (3);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
COMMIT;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Table_map	#	#	table_id: # (test.t1)
binlog.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
binlog.000001	#	Table_map	#	#	table_id: # (test.t1)
binlog.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
binlog.000001	#	Xid	#	#	COMMIT /* XID */
COMMIT;
RESET MASTER;
SET AUTOCOMMIT = 1;
INSERT INTO t1 VALUES (1);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Table_map	#	#	table_id: # (test.t1)
binlog.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
binlog.000001	#	Xid	#	#	COMMIT /* XID */
SET AUTOCOMMIT = 0;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Table_map	#	#	table_id: # (test.t1)
binlog.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
binlog.000001	#	Xid	#	#	COMMIT /* XID */
COMMIT;
RESET MASTER;
SET AUTOCOMMIT = 1;
BEGIN;
INSERT INTO t1 VALUES (2);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
SET AUTOCOMMIT = 0;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
INSERT INTO t1 VALUES (3);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
COMMIT;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Table_map	#	#	table_id: # (test.t1)
binlog.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
binlog.000001	#	Table_map	#	#	table_id: # (test.t1)
binlog.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
binlog.000001	#	Xid	#	#	COMMIT /* XID */
COMMIT;
RESET MASTER;
SET AUTOCOMMIT = 0;
INSERT INTO t1 VALUES (1);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
LOCK TABLES t1 WRITE;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Table_map	#	#	table_id: # (test.t1)
binlog.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
binlog.000001	#	Xid	#	#	COMMIT /* XID */
INSERT INTO t1 VALUES (2);
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Table_map	#	#	table_id: # (test.t1)
binlog.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
binlog.000001	#	Xid	#	#	COMMIT /* XID */
UNLOCK TABLES;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Table_map	#	#	table_id: # (test.t1)
binlog.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
binlog.000001	#	Xid	#	#	COMMIT /* XID */
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Table_map	#	#	table_id: # (test.t1)
binlog.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
binlog.000001	#	Xid	#	#	COMMIT /* XID */
COMMIT;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Table_map	#	#	table_id: # (test.t1)
binlog.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
binlog.000001	#	Xid	#	#	COMMIT /* XID */
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Table_map	#	#	table_id: # (test.t1)
binlog.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
binlog.000001	#	Xid	#	#	COMMIT /* XID */
DROP TABLE t1;
