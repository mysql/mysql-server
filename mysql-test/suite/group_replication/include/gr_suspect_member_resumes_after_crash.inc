#######################################################################################
# WL11570 - GR: options to defer member eviction after a suspicion
#
# In a group of 3 servers, we crash one of them, restart it and make it join the group
# again, which fails. When it finally is expelled from the group, as its suspicion
# on other members times out, it is finally able to rejoin the group.
# Test :
# 0. This test requires a group with 3 servers.
# 1. Set the group_replication_member_expel_timeout parameter to 3600 seconds.
# 2. Create table t1 and insert some data, checking that it is replicated to all
#    servers.
# 3. Crash server 3 and it will restart.
#    This will make server 3 not answer to "I am alive" GCS messages and it will
#    eventually be considered faulty.
# 4. Check that all members are still in the group on servers 1 and 2, which should
#    both be ONLINE.
#    Server 3 should still be in the group but UNREACHABLE.
# 5. Insert data into table t1 on server 2 and check that it is replicated to server 1.
# 6. Restart GR on server 3 and it should fail since its
#    suspicion still hasn't timed out on other members.
# 7. Wait until server 3 is expelled from the group.
# 8. Server 3 was expelled! Start GR on server 3.
# 9. Check that server 3 retrieves the data inserted while
#    it was suspended. Then, use it to insert new data into
#    table t1 which should be replicated to servers 1 and 2.
# 10. Clean up.
#######################################################################################

# Don't test this under valgrind, memory leaks will occur
--source include/not_valgrind.inc
--source include/big_test.inc
--source include/force_restart.inc
--source include/have_group_replication_plugin.inc
--let $rpl_server_count= 3
--source include/group_replication.inc


--echo
--echo ############################################################
--echo # 1. Set group_replication_member_expel_timeout to
--echo #    3600 seconds.
--let $rpl_connection_name= server1
--source include/rpl_connection.inc
SET GLOBAL group_replication_member_expel_timeout = 3600;
--let $server1_local_address= `SELECT @@GLOBAL.group_replication_local_address`

set session sql_log_bin=0;
call mtr.add_suppression("Old incarnation found while trying to add node*.*");
set session sql_log_bin=1;

--let $rpl_connection_name= server2
--source include/rpl_connection.inc
SET GLOBAL group_replication_member_expel_timeout = 3600;

--let $rpl_connection_name= server3
--source include/rpl_connection.inc
SET GLOBAL group_replication_member_expel_timeout = 3600;
set session sql_log_bin=0;
call mtr.add_suppression("read failed");
call mtr.add_suppression("The member was unable to join the group. Local port: *.*");
call mtr.add_suppression("Timeout on wait for view after joining group");
call mtr.add_suppression("Error connecting to all peers. Member join failed. Local port: *.*");
call mtr.add_suppression("Unable to start MySQL Network Provider*.*");
call mtr.add_suppression("Timeout while waiting for the group communication engine to be ready*.*");
call mtr.add_suppression("The group communication engine is not ready for the member to join*.*");

set session sql_log_bin=1;


--echo
--echo ############################################################
--echo # 2. Create a table and insert some data. Check that
--echo #    it is replicated to all servers.
--let $rpl_connection_name= server1
--source include/rpl_connection.inc
CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY);
INSERT INTO t1 VALUES (1);
--source include/rpl_sync.inc


--echo
--echo ############################################################
--echo # 3. Crash server 3 and it will restart.
--let $rpl_connection_name= server3
--source include/rpl_connection.inc
--let $group_replication_local_address= `SELECT @@GLOBAL.group_replication_local_address`
--let $group_replication_group_seeds= `SELECT @@GLOBAL.group_replication_group_seeds`
--let $restart_parameters=restart:--group_replication_local_address=$group_replication_local_address --group_replication_group_seeds=$group_replication_group_seeds --group_replication_group_name=$group_replication_group_name
--replace_result $group_replication_local_address GROUP_REPLICATION_LOCAL_ADDRESS $group_replication_group_seeds GROUP_REPLICATION_GROUP_SEEDS $group_replication_group_name GROUP_REPLICATION_GROUP_NAME
--source include/kill_and_restart_mysqld.inc

--echo #
--echo # Restarting server 3
# Needed as we are not using rpl_restart_server.inc
--let $rpl_server_number= 3
--source include/rpl_reconnect.inc


--echo
--echo ############################################################
--echo # 4. Check that all members are still in the group on
--echo #    servers 1 and 2, which should both be ONLINE.
--echo #    Server 3 should still be in the group but UNREACHABLE.
--let $rpl_connection_name= server1
--source include/rpl_connection.inc
let $wait_condition=SELECT COUNT(*)=2 FROM performance_schema.replication_group_members where MEMBER_STATE="ONLINE";
--source include/wait_condition.inc
let $wait_condition=SELECT COUNT(*)=1 FROM performance_schema.replication_group_members where MEMBER_STATE="UNREACHABLE";
--source include/wait_condition.inc

--let $rpl_connection_name= server2
--source include/rpl_connection.inc
let $wait_condition=SELECT COUNT(*)=2 FROM performance_schema.replication_group_members where MEMBER_STATE="ONLINE";
--source include/wait_condition.inc
let $wait_condition=SELECT COUNT(*)=1 FROM performance_schema.replication_group_members where MEMBER_STATE="UNREACHABLE";
--source include/wait_condition.inc


--echo
--echo ############################################################
--echo # 5. Insert data into table t1 on server 2 and check that
--echo #    it is replicated to server 1.
INSERT INTO t1 VALUES (2);
let $wait_condition=SELECT COUNT(*)=2 FROM t1;
--source include/wait_condition.inc

--let $rpl_connection_name= server1
--source include/rpl_connection.inc
let $wait_condition=SELECT COUNT(*)=2 FROM t1;
--source include/wait_condition.inc


--echo
--echo ############################################################
--echo # 6. Restart GR on server 3 and it should fail since its
--echo #    suspicion still hasn't timed out on other members.
--let $rpl_connection_name= server3
--source include/rpl_connection.inc
--replace_result $group_replication_group_name GROUP_REPLICATION_GROUP_NAME
--eval SET GLOBAL group_replication_group_name= "$group_replication_group_name"
--replace_result $server1_local_address SERVER1_LOCAL_ADDRESS
--eval SET GLOBAL group_replication_group_seeds= "$server1_local_address"
--error ER_GROUP_REPLICATION_CONFIGURATION
START GROUP_REPLICATION;


--echo
--echo ############################################################
--echo # 7. Wait until server 3 is expelled from the group.
--let $rpl_connection_name= server1
--source include/rpl_connection.inc
SET GLOBAL group_replication_member_expel_timeout = DEFAULT;
--let $rpl_connection_name= server2
--source include/rpl_connection.inc
SET GLOBAL group_replication_member_expel_timeout = DEFAULT;

--let $rpl_connection_name= server1
--source include/rpl_connection.inc
let $wait_condition=SELECT COUNT(*)=2 FROM performance_schema.replication_group_members;
--source include/wait_condition.inc

--let $rpl_connection_name= server2
--source include/rpl_connection.inc
let $wait_condition=SELECT COUNT(*)=2 FROM performance_schema.replication_group_members;
--source include/wait_condition.inc


--echo
--echo ############################################################
--echo # 8. Server 3 was expelled! Start GR on server 3.
--let $rpl_connection_name= server3
--source include/rpl_connection.inc
--source include/start_group_replication.inc
SET GLOBAL group_replication_member_expel_timeout = DEFAULT;


--let $rpl_connection_name= server1
--source include/rpl_connection.inc
let $wait_condition=SELECT COUNT(*)=3 FROM performance_schema.replication_group_members where MEMBER_STATE="ONLINE";
--source include/wait_condition.inc

--let $rpl_connection_name= server2
--source include/rpl_connection.inc
let $wait_condition=SELECT COUNT(*)=3 FROM performance_schema.replication_group_members where MEMBER_STATE="ONLINE";
--source include/wait_condition.inc

--let $rpl_connection_name= server3
--source include/rpl_connection.inc
let $wait_condition=SELECT COUNT(*)=3 FROM performance_schema.replication_group_members where MEMBER_STATE="ONLINE";
--source include/wait_condition.inc


--echo
--echo ############################################################
--echo # 9. Check that server 3 retrieves the data inserted while
--echo #    it was suspended. Then, use it to insert new data into
--echo #    table t1 which should be replicated to servers 1 and 2.
--let $rpl_connection_name= server3
--source include/rpl_connection.inc
let $wait_condition=SELECT COUNT(*)=2 FROM t1;
--source include/wait_condition.inc
INSERT INTO t1 VALUES (3);
--source include/rpl_sync.inc
--let $diff_tables=server1:t1, server2:t1, server3:t1
--source include/diff_tables.inc


--echo
--echo ############################################################
--echo # 10. Clean up.
DROP TABLE t1;

--source include/group_replication_end.inc
