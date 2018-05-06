drop table if exists t1;
reset master;
create table t1 (a int);
prepare s from "insert into t1 values (@a),(?)";
set @a=98;
execute s using @a;
prepare s from "insert into t1 values (?)";
set @a=99;
execute s using @a;
prepare s from "insert into t1 select 100 limit ?";
set @a=100;
execute s using @a;
Warnings:
Note	1592	Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT. The statement is unsafe because it uses a LIMIT clause. This is unsafe because the set of rows included cannot be predicted.
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	use `test`; create table t1 (a int)
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	User var	#	#	@`a`=98
binlog.000001	#	Query	#	#	use `test`; insert into t1 values (@a),(98)
binlog.000001	#	Query	#	#	COMMIT
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Query	#	#	use `test`; insert into t1 values (99)
binlog.000001	#	Query	#	#	COMMIT
binlog.000001	#	Query	#	#	BEGIN
binlog.000001	#	Query	#	#	use `test`; insert into t1 select 100 limit 100
binlog.000001	#	Query	#	#	COMMIT
drop table t1;
