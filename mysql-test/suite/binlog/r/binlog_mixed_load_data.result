RESET MASTER;
CREATE TABLE t1 (word CHAR(20) NOT NULL) ENGINE=MYISAM;
LOAD DATA INFILE '../../std_data/words.dat' INTO TABLE t1;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Table_map	#	#	table_id: # (test.t1)
binlog.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
binlog.000001	#	Query	#	#	COMMIT
DROP TABLE t1;
