################################################################################
# Recovery donor failover test
# This test focus on the failover mechanism where a joiner changes it's donor
# whenever the one in use leaves the group while recovery is happening.
#
# To test this, a new member is started but recovery blocked. While at this state
# the donor is killed, forcing the joiner to change to a new one. As there is
# no donor identification as of now, we just unblock and check that recovery
# was successful
#
# Test:
# 0. The test requires three servers: M1, M2 and M3.
# 1. Bootstrap group on M1. Insert some data. Start GR on M2.
# 2. Block the applier on both servers to block recovery.
# 3. Start GR on M3. It should go into RECOVERING state. Identify the DONOR.
# 4. Stop GR on the donor making it leave the group.
# 5. Unblock recovery i.e. Resume the applier on failover member and watch M3 go
#    ONLINE despite the donor exit. Vaidate data on M3.
# 6. Clean up.
################################################################################

--source include/big_test.inc
--source include/have_debug_sync.inc
--let $group_replication_group_name= 55d07150-9a4d-11e3-a5e2-0800200c9a66
--source include/have_group_replication_plugin.inc
--let $rpl_skip_group_replication_start= 1
--let $rpl_server_count= 3
--source include/group_replication.inc

--echo #
--echo # Setup a new 2 member group
--echo #

--connection server1
SET SESSION sql_log_bin = 0;
call mtr.add_suppression("Slave SQL for channel 'group_replication_applier': Relay log read failure: Could not parse relay log event entry.*");
call mtr.add_suppression("The applier thread execution was aborted. Unable to process more transactions, this member will now leave the group.");
call mtr.add_suppression("Fatal error during execution on the Applier process of Group Replication. The server will now leave the group.");
call mtr.add_suppression("The server was automatically set into read only mode after an error was detected.");
call mtr.add_suppression("Skipping leave operation: concurrent attempt to leave the group is on-going.");
SET SESSION sql_log_bin = 1;

--source include/start_and_bootstrap_group_replication.inc

--let $group_replication_number_of_members= 1
--source include/gr_wait_for_number_of_members.inc

#insert some data
CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES (1);

--connection server2
SET SESSION sql_log_bin = 0;
call mtr.add_suppression("Slave SQL for channel 'group_replication_applier': Relay log read failure: Could not parse relay log event entry.*");
call mtr.add_suppression("The applier thread execution was aborted. Unable to process more transactions, this member will now leave the group.");
call mtr.add_suppression("Fatal error during execution on the Applier process of Group Replication. The server will now leave the group.");
call mtr.add_suppression("The server was automatically set into read only mode after an error was detected.");
call mtr.add_suppression("Skipping leave operation: concurrent attempt to leave the group is on-going.");
SET SESSION sql_log_bin = 1;

#extract server uuid for calculations below
--let $server2_uuid= query_get_value(SELECT @@SERVER_UUID, @@SERVER_UUID, 1)

--source include/start_group_replication.inc

--let $group_replication_number_of_members= 2
--source include/gr_wait_for_number_of_members.inc

#Block the applier on both servers to block recovery
SET @@GLOBAL.DEBUG='+d,block_applier_updates';

--connection server1
SET @@GLOBAL.DEBUG='+d,block_applier_updates';

--echo #
--echo # Start recovery on a new member and kill the donor
--echo #

--connection server3
SET SESSION sql_log_bin= 0;
call mtr.add_suppression(".*Error when configuring the asynchronous recovery channel connection *.*");
SET SESSION sql_log_bin= 1;

--let $group_replication_start_member_state= RECOVERING
--source include/start_group_replication.inc

--echo # Find if server 1 or 2 is the donor

#give it time to connect
--sleep 5

#Find what member is the donor
--let $donor_id= 1
--let $failover= 2
if (`SELECT COUNT(*)=1 FROM performance_schema.replication_connection_status WHERE channel_name='group_replication_recovery' AND service_state='ON' AND source_uuid='$server2_uuid'`)
{
  --let $donor_id= 2
  --let $failover= 1
}

--echo # Stop group replication on the donor making it leave the group
--connection server$donor_id
SET DEBUG_SYNC = "now WAIT_FOR applier_read_blocked";
SET @@GLOBAL.DEBUG='-d,block_applier_updates';
SET @@GLOBAL.DEBUG='+d,force_sql_thread_error';
SET DEBUG_SYNC = "now SIGNAL resume_applier_read";
--let $group_replication_member_state= ERROR
--source include/gr_wait_for_member_state.inc
SET @@GLOBAL.DEBUG='-d,force_sql_thread_error';
SET DEBUG_SYNC= 'RESET';
--source include/stop_group_replication.inc

--echo # Unblock recovery and watch the member go online despite the donor exit
--connection server$failover
SET DEBUG_SYNC = "now WAIT_FOR applier_read_blocked";
SET @@GLOBAL.DEBUG='-d,block_applier_updates';
SET DEBUG_SYNC = "now SIGNAL resume_applier_read";

--connection server3

--let $group_replication_member_state= ONLINE
--source include/gr_wait_for_member_state.inc

--let $assert_text= On the recovered member, the table should exist and have 1 elements;
--let $assert_cond= [select count(*) from t1] = 1;
--source include/assert.inc

--let $assert_text= On the recovered member, the executed GTID should be the same as on the donor plus the view changes
--let $assert_cond= "[SELECT @@GLOBAL.GTID_EXECUTED]" = "55d07150-9a4d-11e3-a5e2-0800200c9a66:1-5";
--source include/assert.inc

--echo #
--echo # Cleaning up
--echo #

--connection server$failover
SET DEBUG_SYNC= 'RESET';

--connection server$donor_id
--source include/start_group_replication.inc

DROP TABLE t1;

--source include/group_replication_end.inc
