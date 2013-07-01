include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
SET DEBUG_SYNC= 'after_show_binlog_events SIGNAL on_show_binlog_events WAIT_FOR end';
SHOW BINLOG EVENTS;
[connection slave1]
SET DEBUG_SYNC= 'now WAIT_FOR on_show_binlog_events';
FLUSH LOGS;
SET DEBUG_SYNC= 'now SIGNAL end';
SET DEBUG_SYNC= 'RESET';
[connection slave]
SET DEBUG_SYNC= 'RESET';
include/rpl_end.inc
