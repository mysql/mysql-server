include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
call mtr.add_suppression("Recovery from master pos .*");
call mtr.add_suppression("Corrupted table mysql.slave_worker_info");
call mtr.add_suppression("Could not delete from Slave Workers info repository");
include/stop_slave.inc
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
SET @save.replica_parallel_workers=@@global.replica_parallel_workers;
SET @@global.replica_parallel_workers=2;
CHANGE REPLICATION SOURCE TO SOURCE_HOST='localhost', SOURCE_USER='root', SOURCE_PORT=MASTER_PORT FOR CHANNEL 'ch_a';
Warnings:
Note	1759	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	1760	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
CHANGE REPLICATION SOURCE TO SOURCE_HOST='dummy_host', SOURCE_USER='root', SOURCE_PORT=13010 FOR CHANNEL 'ch_b';
Warnings:
Note	1759	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	1760	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
START SLAVE;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
STOP SLAVE FOR CHANNEL 'ch_b';
Warnings:
Warning	1287	'STOP SLAVE' is deprecated and will be removed in a future release. Please use STOP REPLICA instead
RESET SLAVE FOR CHANNEL 'ch_b';
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
STOP SLAVE FOR CHANNEL 'ch_a';
Warnings:
Warning	1287	'STOP SLAVE' is deprecated and will be removed in a future release. Please use STOP REPLICA instead
ALTER TABLE mysql.slave_worker_info DROP PRIMARY KEY;
RESET SLAVE ALL FOR CHANNEL 'ch_a';
ERROR HY000: Unknown error
ALTER TABLE mysql.slave_worker_info ADD PRIMARY KEY (`channel_name`,`id`);
DELETE FROM mysql.slave_worker_info WHERE channel_name like 'ch_a';
STOP SLAVE IO_THREAD;
Warnings:
Warning	1287	'STOP SLAVE' is deprecated and will be removed in a future release. Please use STOP REPLICA instead
Note	3084	Replication thread(s) for channel 'ch_a' are already stopped.
Note	3084	Replication thread(s) for channel 'ch_b' are already stopped.
CHANGE REPLICATION SOURCE TO SOURCE_HOST='localhost', SOURCE_USER='root', SOURCE_PORT=MASTER_PORT FOR CHANNEL 'ch_b';
Warnings:
Note	1759	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	1760	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
include/assert.inc [Two workers are expected to survive the last RESET SLAVE]
include/start_slave.inc [FOR CHANNEL 'ch_b']

=== RESTART SERVER ===
include/rpl_restart_server.inc [server_number=1]
include/rpl_restart_server.inc [server_number=2]
[connection slave]
CHANGE REPLICATION SOURCE TO SOURCE_HOST='127.0.0.1', SOURCE_USER='root', SOURCE_PORT=MASTER_PORT FOR CHANNEL 'ch_b';
Warnings:
Note	1759	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	1760	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
START SLAVE;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
STOP SLAVE;
Warnings:
Warning	1287	'STOP SLAVE' is deprecated and will be removed in a future release. Please use STOP REPLICA instead
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/rpl_end.inc
