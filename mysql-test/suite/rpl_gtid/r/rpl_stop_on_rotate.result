include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]

############################################################
# 1. Add some data to be applied in the replica
CREATE TABLE t1 (a INT PRIMARY KEY);
INSERT INTO t1 VALUES(1);
include/sync_slave_sql_with_master.inc

############################################################
# 2. Add a debug point so the replica stops at the first sent rotate
#    Stop the replica IO thread so the auto positioning protocol runs
include/stop_slave_io.inc
# Adding debug point 'block_on_master_pos_4_rotate' to @@GLOBAL.debug
# Adding debug point 'block_on_thread_stop_after_awake' to @@GLOBAL.debug
include/start_slave_io.inc

############################################################
# 3. Stop the replica ensuring it stops on the first replica
#    sent rotate that has 4 as the position
SET DEBUG_SYNC= "now WAIT_FOR signal.reach_pos_4_rotate_event";
STOP REPLICA;
[connection server_2]
SET DEBUG_SYNC= "now WAIT_FOR signal.stop_point_after_awake";
SET DEBUG_SYNC= "now SIGNAL signal.rotate_event_continue";
[connection slave]

############################################################
# 4. Start the replica again and check all is good
# Removing debug point 'block_on_master_pos_4_rotate' from @@GLOBAL.debug
# Removing debug point 'block_on_thread_stop_after_awake' from @@GLOBAL.debug
SET DEBUG_SYNC= "RESET";
include/start_slave.inc
[connection master]
INSERT INTO t1 VALUES(2);
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
include/rpl_end.inc
