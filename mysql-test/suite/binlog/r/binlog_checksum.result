set @save_binlog_checksum = @@global.binlog_checksum;
set @save_source_verify_checksum = @@global.source_verify_checksum;
set @@global.binlog_checksum = CRC32;
set @@global.source_verify_checksum = 1;
reset master;
must be binlog.000001
show binary logs;
Log_name	File_size	Encrypted
binlog.000001	#	#
create table t1 (a int);
flush logs;
drop table t1;
RESET MASTER;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	use `test`; create table t1 (a int)
show tables;
Tables_in_test
t1
drop table t1;
set @@global.binlog_checksum = @save_binlog_checksum;
set @@global.source_verify_checksum = @save_source_verify_checksum;
End of the tests
