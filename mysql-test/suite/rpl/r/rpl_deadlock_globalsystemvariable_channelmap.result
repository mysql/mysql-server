include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
include/assert.inc [binlog_encryption option shall be ON]
[connection server_2_1]
SET DEBUG_SYNC='after_locking_global_sys_var_set_binlog_enc SIGNAL sysvar_before_unlock WAIT_FOR cont_set_binlog_enc';
SET GLOBAL binlog_encryption=OFF;
[connection slave]
SET DEBUG_SYNC='now WAIT_FOR sysvar_before_unlock';
[connection server_2_2]
SET DEBUG_SYNC='after_locking_channel_map_in_start_replica SIGNAL after_channelmap_lock WAIT_FOR cont_start_slave';
START SLAVE;
[connection slave]
SET DEBUG_SYNC='now WAIT_FOR after_channelmap_lock';
SET DEBUG_SYNC='now SIGNAL cont_set_binlog_enc';
SET DEBUG_SYNC='now SIGNAL cont_start_slave';
[connection server_2_1]
[connection server_2_2]
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
[connection slave]
SET GLOBAL binlog_encryption=ON;
include/rpl_end.inc
