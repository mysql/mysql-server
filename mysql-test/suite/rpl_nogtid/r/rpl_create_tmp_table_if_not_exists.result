include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE TEMPORARY TABLE IF NOT EXISTS tmp(c1 int);
CREATE TEMPORARY TABLE IF NOT EXISTS tmp(c1 int);
CREATE TEMPORARY TABLE IF NOT EXISTS tmp1 LIKE tmp;
CREATE TEMPORARY TABLE IF NOT EXISTS tmp1 LIKE tmp;
CREATE TEMPORARY TABLE IF NOT EXISTS tmp2 SELECT * FROM tmp;
CREATE TEMPORARY TABLE IF NOT EXISTS tmp2 SELECT * FROM tmp;
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	use `test`; CREATE TEMPORARY TABLE IF NOT EXISTS tmp(c1 int)
master-bin.000001	#	Query	#	#	use `test`; CREATE TEMPORARY TABLE IF NOT EXISTS tmp(c1 int)
master-bin.000001	#	Query	#	#	use `test`; CREATE TEMPORARY TABLE IF NOT EXISTS tmp1 LIKE tmp
master-bin.000001	#	Query	#	#	use `test`; CREATE TEMPORARY TABLE IF NOT EXISTS tmp1 LIKE tmp
master-bin.000001	#	Query	#	#	use `test`; CREATE TEMPORARY TABLE IF NOT EXISTS tmp2 SELECT * FROM tmp
DROP TEMPORARY TABLE IF EXISTS tmp;
DROP TEMPORARY TABLE IF EXISTS tmp1;
DROP TEMPORARY TABLE IF EXISTS tmp2;
include/rpl_end.inc
