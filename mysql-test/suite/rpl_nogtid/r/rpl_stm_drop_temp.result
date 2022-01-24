include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
create database if not exists mysqltest;
use mysqltest;
create temporary table mysqltest.t1 (n int)ENGINE=MyISAM;
create temporary table mysqltest.t2 (n int)ENGINE=MyISAM;
show status like 'Replica_open_temp_tables';
Variable_name	Value
Replica_open_temp_tables	0
drop database mysqltest;
DROP TEMPORARY TABLE IF EXISTS tmp1;
Warnings:
Note	1051	Unknown table 'test.tmp1'
CREATE TEMPORARY TABLE t1 ( a int );
DROP TEMPORARY TABLE t1, t2;
ERROR 42S02: Unknown table 'test.t2'
DROP TEMPORARY TABLE tmp2;
ERROR 42S02: Unknown table 'test.tmp2'
DROP TEMPORARY TABLE t1;
stop slave;
Warnings:
Warning	1287	'STOP SLAVE' is deprecated and will be removed in a future release. Please use STOP REPLICA instead
**** On Master ****
CREATE TEMPORARY TABLE tmp3 (a int);
DROP TEMPORARY TABLE tmp3;
START SLAVE;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/rpl_reset.inc
CREATE TABLE t1 ( i INT );
SHOW STATUS LIKE 'Replica_open_temp_tables';
Variable_name	Value
Replica_open_temp_tables	0
CREATE TEMPORARY TABLE ttmp1 ( i INT );
SHOW STATUS LIKE 'Replica_open_temp_tables';
Variable_name	Value
Replica_open_temp_tables	0
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	use `test`; CREATE TABLE t1 ( i INT )
master-bin.000001	#	Query	#	#	use `test`; CREATE TEMPORARY TABLE ttmp1 ( i INT )
master-bin.000001	#	Query	#	#	use `test`; DROP /*!40005 TEMPORARY */ TABLE IF EXISTS `ttmp1`
DROP TABLE t1;
include/rpl_end.inc
