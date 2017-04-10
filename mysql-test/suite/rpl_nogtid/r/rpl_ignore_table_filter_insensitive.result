include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE DATABASE mydb;
USE mydb;
CREATE TABLE T1 (a INT) ENGINE= MYISAM;
CREATE TABLE t2 (a INT) ENGINE= MYISAM;
INSERT INTO T1 VALUES (1);
INSERT INTO t2 VALUES (1);
# On master
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	CREATE DATABASE mydb
master-bin.000001	#	Query	#	#	use `mydb`; CREATE TABLE T1 (a INT) ENGINE= MYISAM
master-bin.000001	#	Query	#	#	use `mydb`; CREATE TABLE t2 (a INT) ENGINE= MYISAM
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `mydb`; INSERT INTO T1 VALUES (1)
master-bin.000001	#	Query	#	#	COMMIT
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Query	#	#	use `mydb`; INSERT INTO t2 VALUES (1)
master-bin.000001	#	Query	#	#	COMMIT
include/sync_slave_sql_with_master.inc
# On slave
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
slave-bin.000001	#	Query	#	#	CREATE DATABASE mydb
slave-bin.000001	#	Query	#	#	use `mydb`; CREATE TABLE t2 (a INT) ENGINE= MYISAM
slave-bin.000001	#	Query	#	#	BEGIN
slave-bin.000001	#	Query	#	#	use `mydb`; INSERT INTO t2 VALUES (1)
slave-bin.000001	#	Query	#	#	COMMIT
DROP DATABASE mydb;
include/sync_slave_sql_with_master.inc
Replicate_Ignore_Table: mydb.t3,mydb.ţ2
include/rpl_end.inc
