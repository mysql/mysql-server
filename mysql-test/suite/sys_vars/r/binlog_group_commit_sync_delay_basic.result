# Default values
SELECT @@GLOBAL.binlog_group_commit_sync_delay;
@@GLOBAL.binlog_group_commit_sync_delay
0
SELECT @@SESSION.binlog_group_commit_sync_delay;
ERROR HY000: Variable 'binlog_group_commit_sync_delay' is a GLOBAL variable
# Valid values
# Invalid values: there shall be warnings about truncation
Warnings:
Warning	1292	Truncated incorrect binlog_group_commit_sync_delay value: '-1'
include/assert.inc ["Assert that binlog_group_commit_sync_delay was truncated to MIN allowed value"]
Warnings:
Warning	1292	Truncated incorrect binlog_group_commit_sync_delay value: '1000001'
include/assert.inc ["Assert that binlog_group_commit_sync_delay was truncated to MAX allowed value"]
