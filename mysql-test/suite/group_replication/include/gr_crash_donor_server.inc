################################################################################
# This test verifies whether the group_replication works fine if the
# donor server is killed when the joiner is already on recovery phase-2.
#
# Test:
# 0. The test requires three servers: M1,M2 and M3.
# 1. Start GR on servers M1 and M2.
# 2. Execute some operations on the group through server M1
#    using procedure dml_operations.
# 3. Join M3 to the group and wait until the server is in Recovery.
# 4. Identify the donor server for M3.
# 5. perform the transactions on the group.
# 6. Wait until recovery phase-1 on M3 ends.
# 7. Kill and restart the donor server during phase-2 of M3 recovery.
# 8.Wait until all the members are online.
# 9.Verify that all the members have same data.
# 10.Cleanup
################################################################################

# This test does crashes servers, thence we skip it on valgrind.
--source include/not_valgrind.inc
--source include/big_test.inc
--source include/force_restart.inc

--source include/set_privilege_checks_user_as_system_user.inc
--source include/have_group_replication_plugin.inc
--let $rpl_server_count= 3
--let $rpl_skip_group_replication_start= 1
--source include/group_replication.inc

# START GR on Two servers M1 and M2
--let $rpl_connection_name= server1
--source include/rpl_connection.inc

SET sql_log_bin=0;
CREATE TABLE t1(a int primary key);
SET sql_log_bin=1;

# Extract the server_uuid
--let $server1_uuid= query_get_value(SELECT @@SERVER_UUID, @@SERVER_UUID, 1)

--source include/start_and_bootstrap_group_replication.inc

--let $rpl_connection_name= server2
--source include/rpl_connection.inc

SET sql_log_bin=0;
CREATE TABLE t1(a int primary key);
SET sql_log_bin=1;

# Extract the server_uuid
--let $server2_uuid= query_get_value(SELECT @@SERVER_UUID, @@SERVER_UUID, 1)

--source include/start_group_replication.inc

# Perform the transactions on group through server1 so
# that when M3 joins it will be in recovery for some time.

--let $rpl_connection_name= server1
--source include/rpl_connection.inc

# Create the procedure to perform dml operations
delimiter $$;
CREATE PROCEDURE dml_operations()
     BEGIN
     declare x INT;
     set x=1;
     while x<100 do
     insert into t1 values (x);
     update t1 set a=x+200 where a=x;
     delete from t1 where a<250;
     set x=x+1;
     end  while;
     end$$
delimiter ;$$
--echo

--echo ----call procedure----
call dml_operations();
--echo

# Wait for the data to get synced on server2
--source include/rpl_sync.inc

# JOIN M3 to the group
--let $rpl_connection_name= server3
--source include/rpl_connection.inc

SET sql_log_bin=0;
CREATE TABLE t1(a int primary key);
SET sql_log_bin=1;
LOCK TABLES t1 READ;

--let $rpl_connection_name= server_3
--source include/rpl_connection.inc

SET SESSION sql_log_bin= 0;
call mtr.add_suppression("There was an error when connecting to the donor server.*");
call mtr.add_suppression("For details please check performance_schema.replication_connection_status table and error log messages of Replica I/O for channel group_replication_recovery.");
call mtr.add_suppression("Replica I/O for channel 'group_replication_recovery': Source command COM_REGISTER_REPLICA failed: failed registering on source, reconnecting to try again.*");
SET SESSION sql_log_bin= 1;

--let $group_replication_start_member_state= RECOVERING
--source include/start_group_replication.inc

# Identify the donor_server for M3

--let $wait_timeout= 100
--let $wait_condition= SELECT COUNT(*)=1 FROM performance_schema.replication_connection_status WHERE channel_name='group_replication_recovery' AND service_state='ON'
--source include/wait_condition.inc

let $donor_id= 1;
let $other_id= 2;

if(`SELECT COUNT(*)=1 FROM performance_schema.replication_connection_status WHERE channel_name='group_replication_recovery' AND service_state='ON' AND source_uuid='$server2_uuid'`)
{
  let $donor_id= 2;
  let $other_id= 1;
}

# Connect to server which is neither M3 nor the donor server and
# Create procedure to perform dml operations and call it using send.
# So that these operations gets executed while server M3 is in recovery.

--let $rpl_connection_silent= 1
--let $rpl_connection_name= server$other_id
--source include/rpl_connection.inc
# Create the procedure to perform dml operations
delimiter $$;
CREATE PROCEDURE dml_operations_2()
     BEGIN
     declare x INT;
     set x=100;
     while x<200 do
     insert into t1 values (x);
     update t1 set a=x+200 where a=x;
     delete from t1 where a<320;
     set x=x+1;
     end  while;
     end$$
delimiter ;$$
--echo

--echo ----call procedure----
send call dml_operations_2();
--echo

# Wait until Recovery phase1 ends.(until the service_state of group_replication_recovery goes to OFF state.)
--let $rpl_connection_name= server3
--source include/rpl_connection.inc
UNLOCK TABLES;
--let $wait_timeout=200
--let $wait_condition= SELECT COUNT(*)=1 FROM performance_schema.replication_connection_status WHERE channel_name='group_replication_recovery' AND service_state='OFF';
--source include/wait_condition.inc

# Kill the donor server during phase 2 of M3's recovery
--let $rpl_connection_name= server$donor_id
--source include/rpl_connection.inc

SET sql_log_bin=0;
call mtr.add_suppression(".*Replica SQL for channel 'group_replication_applier': ... The replica coordinator and worker threads are stopped, possibly leaving data in inconsistent state*");
SET sql_log_bin=1;

# kill donor server
--echo # killing
--let $group_replication_local_address= `SELECT @@GLOBAL.group_replication_local_address`
--let $group_replication_group_seeds= `SELECT @@GLOBAL.group_replication_group_seeds`
--let $restart_parameters=restart:--group_replication_local_address=$group_replication_local_address --group_replication_group_seeds=$group_replication_group_seeds --group_replication_group_name=$group_replication_group_name
--replace_result $group_replication_local_address GROUP_REPLICATION_LOCAL_ADDRESS $group_replication_group_seeds GROUP_REPLICATION_GROUP_SEEDS $group_replication_group_name GROUP_REPLICATION_GROUP_NAME
--source include/kill_and_restart_mysqld.inc
--echo # restarting

# Needed as we are not using rpl_restart_server.inc
--let $rpl_server_number= $donor_id
--source include/rpl_reconnect.inc

--let $rpl_connection_name= server$other_id
--source include/rpl_connection.inc
reap;
# Wait until group has only 2 members, that is, donor server's death is detected by
# the group.
--echo # check that there are 2 mebers in the group
--let $group_replication_number_of_members= 2
--source include/gr_wait_for_number_of_members.inc

--sleep 5

# Start GR on killed and restarted donor server and wait until all the servers are ONLINE.
# Start GR on donor server
--let $rpl_connection_name= server$donor_id
--source include/rpl_connection.inc

--let $wait_timeout= 200
--source include/start_group_replication.inc

--let $rpl_connection_name= server$other_id
--source include/rpl_connection.inc

--let $wait_timeout= 200
--let $wait_condition= SELECT COUNT(*) = 3 FROM performance_schema.replication_group_members where MEMBER_STATE='ONLINE';
--source include/wait_condition.inc

--source include/rpl_sync.inc

# Verify that table t1 has same data on all the servers.
--let $diff_tables=server1:t1, server2:t1, server3:t1
--source include/diff_tables.inc

# Clean up.
drop table t1;
drop procedure dml_operations;
drop procedure dml_operations_2;
--let $skip_restore_connection= 0
--source include/group_replication_end.inc
