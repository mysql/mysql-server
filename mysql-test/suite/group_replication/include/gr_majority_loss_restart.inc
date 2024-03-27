################################################################################
# Validate that when a group looses the majority, if the faulty member tries to
# rejoin it will be disallowed to unblock the group and user needs to use
# group_replication_force_members option.
#
# 0. Start with 2 servers running GR
# 1. Crash server 2.
# 2. Execute a transaction on server 1, which will block
#    since the group does not have majority.
# 3. Assert that group is still blocked.
# 4. Restart server 2 with group_replication_start_on_boot=ON,
#    server 2 will be refused to join the group.
# 5. Unblock group with server1 as a single member.
# 6. Check that statement did unblock and was applied on
#    server1.
# 7. Rejoin server2
# 8. Execute a transaction on both members.
# 9. Validate data.
#10. Clean up.
################################################################################
# This test does crashes servers, thence we skip it on valgrind.
--source include/not_valgrind.inc
--source include/big_test.inc
--source include/force_restart.inc
--source include/have_group_replication_plugin.inc
--source include/group_replication.inc

--let $rpl_connection_name= server1
--source include/connection.inc
--let $member1_uuid= query_get_value(SELECT @@SERVER_UUID, @@SERVER_UUID, 1)
--let $member1_group_replication_local_address= `SELECT @@GLOBAL.group_replication_local_address`

set session sql_log_bin=0;
call mtr.add_suppression("Old incarnation found while trying to add node*.*");
set session sql_log_bin=1;

--let $rpl_connection_name= server2
--source include/connection.inc
--let $member2_uuid= query_get_value(SELECT @@SERVER_UUID, @@SERVER_UUID, 1)
--let $member2_group_replication_local_address= `SELECT @@GLOBAL.group_replication_local_address`
--let $member2_group_replication_group_seeds= `SELECT @@GLOBAL.group_replication_group_seeds`

SET SESSION sql_log_bin= 0;
call mtr.add_suppression("Timeout on wait for view after joining group");
call mtr.add_suppression("read failed");
call mtr.add_suppression("The member was unable to join the group. Local port: *.*");
call mtr.add_suppression("Error connecting to all peers. Member join failed. Local port: *.*");
SET SESSION sql_log_bin= 1;


--echo
--echo ############################################################
--echo # 1. Crash server 2.
--let $rpl_connection_name= server2
--source include/connection.inc

--source include/kill_and_restart_mysqld.inc
--source include/rpl/reconnect.inc

--let $rpl_connection_name= server1
--source include/connection.inc

--let $group_replication_member_state= UNREACHABLE
--let $group_replication_member_id= $member2_uuid
--source include/gr_wait_for_member_state.inc


--echo
--echo ############################################################
--echo # 2. Execute a transaction on server 1, which will block
--echo #    since the group does not have majority.
--let $rpl_connection_name= server_1
--source include/connection.inc
# This transaction will be blocked since group lost its majority.
# Though it needs to be delivered to the group when unblocked.
--send CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY)

--let $rpl_connection_name= server1
--source include/connection.inc
--let $wait_condition= SELECT PROCESSLIST_STATE="waiting for handler commit" FROM performance_schema.threads WHERE processlist_INFO LIKE "CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY)"
--source include/wait_condition.inc


--echo
--echo ############################################################
--echo # 3. Assert that group is still blocked.
--let $group_replication_number_of_members= 2
--source include/gr_wait_for_number_of_members.inc

--let $assert_text= server 2 is marked as unreachable
--let $assert_cond= [SELECT COUNT(*) FROM performance_schema.replication_group_members WHERE member_id="$member2_uuid" AND member_state="UNREACHABLE"] = 1
--source include/assert.inc


--echo
--echo ############################################################
--echo # 4. Restart server 2 with group_replication_start_on_boot=ON,
--echo #    server 2 will be refused to join the group.
--let $rpl_connection_name= server2
--source include/connection.inc
--let $restart_parameters=restart:--group_replication_local_address=$member2_group_replication_local_address --group_replication_group_seeds=$member2_group_replication_group_seeds --group_replication_start_on_boot=ON --group-replication-group-name=$group_replication_group_name
--replace_result $group_replication_group_name GROUP_REPLICATION_GROUP_NAME $member2_group_replication_local_address GROUP_REPLICATION_LOCAL_ADDRESS $member2_group_replication_group_seeds GROUP_REPLICATION_GROUP_SEEDS
--source include/kill_and_restart_mysqld.inc

# Give it some time to try and fail
--sleep 75

--let $rpl_server_number= 2
--source include/rpl/reconnect.inc
--let $group_replication_member_state= OFFLINE
--source include/gr_wait_for_member_state.inc

--let $rpl_connection_name= server1
--source include/connection.inc
--let $assert_text= server 2 is marked as unreachable
--let $assert_cond= [SELECT COUNT(*) FROM performance_schema.replication_group_members WHERE member_id="$member2_uuid" AND member_state="UNREACHABLE"] = 1
--source include/assert.inc


--echo
--echo ############################################################
--echo # 5. Unblock group with server1 as a single member.
--let $local_address_server1= `SELECT @@GLOBAL.group_replication_local_address`
--replace_result $local_address_server1 SERVER1_ADDRESS
--eval SET GLOBAL group_replication_force_members= "$local_address_server1"


--echo
--echo ############################################################
--echo # 6. Check that statement did unblock and was applied on
--echo #    server1.
--let $rpl_connection_name= server_1
--source include/connection.inc
--reap

--let $rpl_connection_name= server1
--source include/connection.inc
--let $wait_condition= SELECT COUNT(*)=1 FROM information_schema.tables WHERE TABLE_SCHEMA="test" AND TABLE_NAME="t1";
--source include/wait_condition.inc


--echo
--echo ############################################################
--echo # 7. Rejoin server2
--let $rpl_connection_name= server2
--source include/connection.inc
# Ensure that GR is already stopped on server2.
--let $skip_assert_read_only= 1
--source include/stop_group_replication.inc
--source include/start_group_replication.inc


--echo
--echo ############################################################
--echo # 8. Execute a transaction on both members.
--let $rpl_connection_name= server1
--source include/connection.inc
INSERT INTO t1 VALUES (1);

--let $rpl_connection_name= server2
--source include/connection.inc
INSERT INTO t1 VALUES (2);

--source include/rpl/sync.inc


--echo
--echo ############################################################
--echo # 9. Validate data.
--let $rpl_connection_name= server1
--source include/connection.inc
--let $assert_text= 'There are two values in table t1'
--let $assert_cond= [SELECT COUNT(*) AS count FROM t1, count, 1] = 2
--source include/assert.inc
--source include/rpl/deprecated/show_binlog_events.inc

--let $rpl_connection_name= server2
--source include/connection.inc
--let $assert_text= 'There are two values in table t1'
--let $assert_cond= [SELECT COUNT(*) AS count FROM t1, count, 1] = 2
--source include/assert.inc

--let $binlog_file= LAST
--source include/rpl/deprecated/show_binlog_events.inc

--let $diff_tables= server1:test.t1, server2:test.t1
--source include/diff_tables.inc


--echo
--echo ############################################################
--echo # 10. Clean up.
DROP TABLE t1;

--source include/group_replication_end.inc
