include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
**** On Master ****
CREATE TABLE t1 (a INT, b LONG);
INSERT INTO t1 VALUES (1,1), (2,2);
INSERT INTO t1 VALUES (3,UUID()), (4,UUID());
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	use `test`; CREATE TABLE t1 (a INT, b LONG)
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (1,1), (2,2)
master-bin.000001	#	Query	#	#	COMMIT
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.t1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Query	#	#	COMMIT
include/sync_slave_sql_with_master.inc
**** On Slave ****
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
slave-bin.000001	#	Query	#	#	use `test`; CREATE TABLE t1 (a INT, b LONG)
slave-bin.000001	#	Query	#	#	BEGIN
slave-bin.000001	#	Query	#	#	use `test`; INSERT INTO t1 VALUES (1,1), (2,2)
slave-bin.000001	#	Query	#	#	COMMIT
slave-bin.000001	#	Query	#	#	BEGIN
slave-bin.000001	#	Table_map	#	#	table_id: # (test.t1)
slave-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
slave-bin.000001	#	Query	#	#	COMMIT
DROP TABLE IF EXISTS t1;
include/rpl_end.inc
