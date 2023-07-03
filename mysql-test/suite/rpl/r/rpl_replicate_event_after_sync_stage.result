include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
#
# Case 1: The events cannot be replicated before they are synced to disk
#         when sync_binlog is 1.
#
[connection master]
SET @save_sync_binlog = @@GLOBAL.sync_binlog;
SET GLOBAL sync_binlog = 1;
SET DEBUG_SYNC = 'before_sync_binlog_file WAIT_FOR continue';
CREATE TABLE t1(c1 INT);
[connection slave]
include/assert.inc ["CREATE TABLE t1 is not be replicated"]
[connection default]
SET DEBUG_SYNC = "now SIGNAL continue";
[connection master]
[connection master]
#
# Case 2: The events are able to be replicated before they are synced to
#         disk when sync_binlog is 1.
#
SET GLOBAL sync_binlog = 0;
DROP TABLE t1;
SET DEBUG_SYNC = "now SIGNAL continue";
[connection master]
SET DEBUG_SYNC = 'RESET';
SET GLOBAL sync_binlog = @save_sync_binlog;
include/rpl_end.inc
