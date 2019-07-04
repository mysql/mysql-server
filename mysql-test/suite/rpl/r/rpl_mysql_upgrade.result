include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
call mtr.add_suppression("Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT");
DROP DATABASE IF EXISTS `#mysql50#mysqltest-1`;
CREATE DATABASE `#mysql50#mysqltest-1`;
include/sync_slave_sql_with_master.inc
include/stop_slave_io.inc
include/rpl_restart_server.inc [server_number=1 parameters: --upgrade=FORCE]
include/start_slave_io.inc
include/stop_slave.inc
include/rpl_restart_server.inc [server_number=1 parameters: --upgrade=FORCE]
Master position has been changed
DROP DATABASE `#mysql50#mysqltest-1`;
DROP DATABASE `#mysql50#mysqltest-1`;
include/rpl_end.inc
