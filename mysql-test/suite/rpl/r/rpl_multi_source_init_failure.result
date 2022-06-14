include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
CALL mtr.add_suppression("A slave with the same server_uuid/server_id as this slave has connected to the master");
CALL mtr.add_suppression("Error during --relay-log-recovery.*");
CALL mtr.add_suppression("Server was not able to find a rotate event from master server.*");
CALL mtr.add_suppression("Recovery from master pos.* and file");
include/stop_slave.inc
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
STOP SLAVE FOR CHANNEL 'ch_a';
Warnings:
Warning	1287	'STOP SLAVE' is deprecated and will be removed in a future release. Please use STOP REPLICA instead
include/rpl_restart_server.inc [server_number=2 parameters: --skip-replica-start]
[connection slave]
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/rpl_end.inc
