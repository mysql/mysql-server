include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
call mtr.add_suppression("Slave I/O .*: Master command COM_REGISTER_SLAVE failed: .*");
include/stop_slave.inc
[connection master]
CREATE TABLE t1 (c1 INT);
DROP TABLE t1;
[connection slave]
#
# Active injected error during getting server id from master
#
# Adding debug point 'get_source_server_id.ER_NET_READ_INTERRUPTED' to @@GLOBAL.debug
START SLAVE;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_to_start.inc
[connection master]
include/sync_slave_io_with_master.inc
include/assert_grep.inc [Make sure there is not the error "fatal error is encountered when it try to get the value of SERVER_ID variable from master" in slave's error log.]
include/assert_grep.inc [Make sure there is the warning "Get master SERVER_ID failed with error" in slave's error log.]
include/stop_slave.inc
# Removing debug point 'get_source_server_id.ER_NET_READ_INTERRUPTED' from @@GLOBAL.debug
include/start_slave.inc
include/rpl_end.inc
