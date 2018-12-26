include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
#
# Verify that SET @@global.binlog_format and SET @@persist.binlog_format
# must be disallowed if any replication channel applier is running.
#
SET @@global.binlog_format=STATEMENT;
ERROR HY000: Changing @@global.binlog_format or @@persist.binlog_format is disallowed when any replication channel applier thread is running. You could execute STOP SLAVE SQL_THREAD and try again.
SET @@persist.binlog_format=STATEMENT;
ERROR HY000: Changing @@global.binlog_format or @@persist.binlog_format is disallowed when any replication channel applier thread is running. You could execute STOP SLAVE SQL_THREAD and try again.
include/stop_slave_io.inc
SET @@global.binlog_format=STATEMENT;
ERROR HY000: Changing @@global.binlog_format or @@persist.binlog_format is disallowed when any replication channel applier thread is running. You could execute STOP SLAVE SQL_THREAD and try again.
SET @@persist.binlog_format=STATEMENT;
ERROR HY000: Changing @@global.binlog_format or @@persist.binlog_format is disallowed when any replication channel applier thread is running. You could execute STOP SLAVE SQL_THREAD and try again.
include/start_slave_io.inc
#
# Verify that SET @@global.binlog_format takes effect if
# no replication channel applier is running.
#
include/stop_slave_sql.inc
set @saved_binlog_format=@@global.binlog_format;
SET @@global.binlog_format=STATEMENT;
include/assert.inc [assert that @@global.binlog_format is set to STATEMENT]
SET @@global.binlog_format=ROW;
include/assert.inc [assert that @@global.binlog_format is set to ROW]
SET @@global.binlog_format=MIXED;
include/assert.inc [assert that @@global.binlog_format is set to MIXED]
include/start_slave_sql.inc
include/stop_slave.inc
SET @@global.binlog_format=STATEMENT;
include/assert.inc [assert that @@global.binlog_format is set to STATEMENT]
SET @@global.binlog_format=ROW;
include/assert.inc [assert that @@global.binlog_format is set to ROW]
SET @@global.binlog_format=MIXED;
include/assert.inc [assert that @@global.binlog_format is set to MIXED]
set global binlog_format=@saved_binlog_format;
include/assert.inc [assert that @@global.binlog_format is set to @saved_binlog_format]
include/start_slave.inc
[connection master]
include/rpl_end.inc
