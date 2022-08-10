SET @@global.expire_logs_days = 0;
Warnings:
Warning	1287	'@@expire_logs_days' is deprecated and will be removed in a future release. Please use binlog_expire_logs_seconds instead.
SET @@global.binlog_expire_logs_auto_purge = OFF;
FLUSH LOGS;
FLUSH LOGS;
SET GLOBAL binlog_expire_logs_seconds=3;
FLUSH LOGS;
SET @@global.binlog_expire_logs_auto_purge = ON;
FLUSH LOGS;
SET @@global.binlog_expire_logs_auto_purge = ON;
SET @@global.binlog_expire_logs_seconds = 0;
FLUSH LOGS;
FLUSH LOGS;
FLUSH LOGS;
SET @@global.binlog_expire_logs_auto_purge = OFF;
FLUSH LOGS;
FLUSH LOGS;
PURGE BINARY LOGS TO 'SECOND_BINLOG_FILE';
SET @@global.binlog_expire_logs_auto_purge = OFF;
SET @@global.binlog_expire_logs_auto_purge = 0;
FLUSH LOGS;
FLUSH LOGS;
FLUSH LOGS;
PURGE BINARY LOGS BEFORE 'DATE';
FLUSH LOGS;
SET GLOBAL binlog_expire_logs_seconds=2592000;
SET GLOBAL binlog_expire_logs_auto_purge=1;
SET GLOBAL expire_logs_days=0;
Warnings:
Warning	1287	'@@expire_logs_days' is deprecated and will be removed in a future release. Please use binlog_expire_logs_seconds instead.
RESET MASTER;
