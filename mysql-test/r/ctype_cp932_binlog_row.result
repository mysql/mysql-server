drop table if exists t1;
set names cp932;
set character_set_database = cp932;
Warnings:
Warning	1681	Updating 'character_set_database' is deprecated. It will be made read-only in a future release.
RESET MASTER;
CREATE TABLE t1(f1 blob);
PREPARE stmt1 FROM 'INSERT INTO t1 VALUES(?)';
SET @var1= x'8300';
EXECUTE stmt1 USING @var1;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	use `test`; CREATE TABLE t1(f1 blob)
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Table_map	#	#	table_id: # (test.t1)
binlog.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
binlog.000001	#	Xid	#	#	COMMIT /* XID */
SELECT HEX(f1) FROM t1;
HEX(f1)
8300
DROP table t1;
