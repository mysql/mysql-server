include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
#
# TC1. Apply OLD-generated relay log on NEW server
#
[connection slave]
call mtr.add_suppression("Slave SQL.*Error executing row event: .Table .test.t. doesn.t exist., Error_code: MY-001146");
include/setup_fake_relay_log.inc
Setting up fake replication from MYSQL_TEST_DIR/std_data/rpl_unfiltered_hidden_gcol.000001
START SLAVE SQL_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_param.inc [Slave_SQL_Running_State]
include/assert.inc [Slave was able to apply the provided relay log.]
DROP TABLE t;
STOP SLAVE SQL_THREAD;
Warnings:
Warning	1287	'STOP SLAVE' is deprecated and will be removed in a future release. Please use STOP REPLICA instead
include/cleanup_fake_relay_log.inc
include/start_slave.inc
include/rpl_end.inc
