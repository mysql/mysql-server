################################################################################
# Make a member to be blocked on join during distributed recovery.
#
# Requirements:
#  1) A table t1 with a auto-increment must exist.
#     CREATE TABLE t1 (c1 INT NOT NULL AUTO_INCREMENT PRIMARY KEY)
#  2) Group must have 2 members, member 2 will be one that will be blocked.
#
# Include:
#  0. The test requires two servers: M1 and M2.
#  1. Stop M2.
#  2. Commit a transaction on table t1 on M1.
#  3. Lock table t1 on M2.
#  3. Join M2 to the group, it will be blocked.
#
#  to unblock M2 do:
#    --source ../include/gr_recovery_channel_resume.inc
#
################################################################################

--let $include_filename= gr_recovery_channel_block.inc
--source include/begin_include_file.inc

--let $_saved_connection= $CURRENT_CONNECTION
--disable_query_log

--let $rpl_connection_name= server2
--source include/connection.inc
--source include/rpl/sync.inc
--source include/stop_group_replication.inc

--let $rpl_connection_name= server1
--source include/connection.inc
INSERT INTO t1 VALUES (NULL);

--let $rpl_connection_name= server_2
--source include/connection.inc
LOCK TABLE t1 READ;

--let $rpl_connection_name= server2
--source include/connection.inc
--let $group_replication_start_member_state= RECOVERING
--source include/start_group_replication.inc

--let $wait_condition= SELECT COUNT(*)=1 FROM performance_schema.replication_connection_status where CHANNEL_NAME="group_replication_recovery" and SERVICE_STATE="ON"
--source include/wait_condition.inc

--enable_query_log
--connection $_saved_connection

--let $include_filename= gr_recovery_channel_block.inc
--source include/end_include_file.inc
