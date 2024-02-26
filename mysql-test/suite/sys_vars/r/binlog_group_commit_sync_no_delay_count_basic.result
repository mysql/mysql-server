# Default values
SELECT @@GLOBAL.binlog_group_commit_sync_no_delay_count;
@@GLOBAL.binlog_group_commit_sync_no_delay_count
0
SELECT @@SESSION.binlog_group_commit_sync_no_delay_count;
ERROR HY000: Variable 'binlog_group_commit_sync_no_delay_count' is a GLOBAL variable
# Valid values
# Invalid values: there shall be warnings about truncation
Warnings:
Warning	1292	Truncated incorrect binlog_group_commit_sync_no_delay_count value: '-1'
include/assert.inc ["Assert that binlog_group_commit_sync_no_delay_count was truncated to MIN allowed value"]
Warnings:
Warning	1292	Truncated incorrect binlog_group_commit_sync_no_delay_count value: '100001'
include/assert.inc ["Assert that binlog_group_commit_sync_no_delay_count was truncated to MAX allowed value"]
