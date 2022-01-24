include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
#
# New --dump-replica, --apply-replica-statements functionality
#
use test;
CHANGE MASTER TO MASTER_LOG_FILE='master-bin.000001', MASTER_LOG_POS=BINLOG_START;
STOP SLAVE;
CHANGE MASTER TO MASTER_LOG_FILE='master-bin.000001', MASTER_LOG_POS=BINLOG_START;
START SLAVE;
STOP SLAVE;
CHANGE MASTER TO MASTER_HOST='127.0.0.1', MASTER_PORT=MASTER_MYPORT, MASTER_LOG_FILE='master-bin.000001', MASTER_LOG_POS=BINLOG_START;
START SLAVE;
include/rpl_end.inc
