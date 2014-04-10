include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
create database mysqltest;
create table mysqltest.t1 ( n int);
insert into mysqltest.t1 values(1)/
insert into mysqltest.t1 values(2);
insert into mysqltest.t1 values(3);
insert into mysqltest.t1 values(4);
insert into mysqltest.t1 values(5)/
include/sync_slave_sql_with_master.inc
select * from mysqltest.t1;
n
1
2
3
4
5
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	create database mysqltest
master-bin.000001	#	Query	#	#	use `test`; create table mysqltest.t1 ( n int)
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; insert into mysqltest.t1 values(1)
master-bin.000001	#	Query	#	#	COMMIT
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; insert into mysqltest.t1 values(2)
master-bin.000001	#	Query	#	#	COMMIT
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; insert into mysqltest.t1 values(3)
master-bin.000001	#	Query	#	#	COMMIT
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; insert into mysqltest.t1 values(4)
master-bin.000001	#	Query	#	#	COMMIT
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `test`; insert into mysqltest.t1 values(5)
master-bin.000001	#	Query	#	#	COMMIT
drop database mysqltest;
include/rpl_end.inc
