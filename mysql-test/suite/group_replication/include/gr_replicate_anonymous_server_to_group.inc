# ==== Purpose ====
#
# This test checks that the replica parameter ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS works when
# replicating into a Group replication group.
#
# ==== Requirements ====
#
# + TC1: **ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS** works when replicating into a group
#
# ==== Implementation ====
#
# 0. This test works with 2 servers in multi primary mode and a third server
#    replicating into to the group trough async replication
#
# 1. Create the group by starting GR on server 1 and 2
# 2. Disable GTIDs on server 3
# 3. Setup a asynchronous replication connection from server 3 into the group (server 1)
#    Calculate the source_uuid
# 4. Execute some transactions on server 3 and sync with server 1
# 5. Execute a transaction on the group and sync
# 6. GTID_EXECUTED on server 1 must contain the transactions from server 3.
#    Validate data on server 1.
# 7. GTID_EXECUTED on server 2 must contain the transactions from server 3.
#    Validate data on server 2.
# 8. Cleanup
#
# ==== References ====
#
# WL#12819:GTIDs: Replicate from GTID disabled source to GTID enabled replica directly
#
# ==== Usage ====
#
#   --let $_rename_parameter = LOCAL | UUID
#   --source ../include/gr_replicate_anonymous_server_to_group.inc
#
# Parameters:
#   $_rename_parameter
#     The ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS parameter for the server that receives the transactions
#

--source include/have_group_replication_plugin.inc
--let $rpl_skip_group_replication_start= 1
--let $rpl_server_count= 3
--source include/group_replication.inc

--echo
--echo ############################################################
--echo #  1. Setup Group Replication on server 1 and 2.

--let $rpl_connection_name= server1
--source include/rpl_connection.inc
--source include/start_and_bootstrap_group_replication.inc

--let $rpl_connection_name= server2
--source include/rpl_connection.inc
--source include/start_group_replication.inc

--echo
--echo ############################################################
--echo #  2. Disable GTIDs on server 3

--let $rpl_connection_name= server3
--source include/rpl_connection.inc

--let $rpl_gtid_mode= OFF
--let $rpl_server_numbers= 3
--source include/rpl_set_gtid_mode.inc

--echo
--echo ############################################################
--echo #  3. Setup a asynchronous replication connection from server 3
--echo #     into the group (server 1)
--echo #     Calculate the source_uuid

--let $rpl_connection_name= server1
--source include/rpl_connection.inc

--let $_source_uuid = $_rename_parameter
--let $__rename_parameter = "$_rename_parameter"
if ($_rename_parameter == 'LOCAL')
{
       --let $_source_uuid = `SELECT @@SERVER_UUID`
       --let $__rename_parameter = $_rename_parameter
}

--replace_result $SERVER_MYPORT_3 SERVER_3_PORT
--eval CHANGE REPLICATION SOURCE TO SOURCE_HOST="127.0.0.1", SOURCE_USER="root", SOURCE_PASSWORD="", SOURCE_PORT=$SERVER_MYPORT_3, ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS=$__rename_parameter
--source include/start_slave.inc


--echo
--echo ###################################################################
--echo #  4. Execute some transactions on server 3 and sync with server 1

--let $rpl_connection_name= server3
--source include/rpl_connection.inc

CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO t1 VALUES (0);
INSERT INTO t1 VALUES (1);
UPDATE t1 SET c1=2 WHERE c1=0;

--let $sync_slave_connection= server1
--let $use_gtids= 0
--source include/sync_slave_sql_with_master.inc
--let $use_gtids= 1

--echo
--echo ############################################################
--echo #  5. Execute a transaction on the group and sync
--let $rpl_connection_name= server1
--source include/rpl_connection.inc

INSERT INTO t1 VALUES (6);
--source include/rpl_sync.inc

--echo
--echo ############################################################
--echo #  6. GTID_EXECUTED on server 1 must contain the transactions
--echo #     from server 3.
--echo #     Validate data on server 1.
--let $rpl_connection_name= server1
--source include/rpl_connection.inc

--let $assert_text= GTID_EXECUTED must contain server 4 GTID_EXECUTED from server 3 with server 1 UUID
--let $assert_cond= [SELECT GTID_SUBSET("$_source_uuid:1-4", @@GLOBAL.GTID_EXECUTED)] = 1
--source include/assert.inc

--let $assert_text= 'There is a value 1 in table t1'
--let $assert_cond= [SELECT COUNT(*) AS count FROM t1 WHERE t1.c1 = 1, count, 1] = 1
--source include/assert.inc
--let $assert_text= 'There is a value 2 in table t1'
--let $assert_cond= [SELECT COUNT(*) AS count FROM t1 WHERE t1.c1 = 2, count, 1] = 1
--source include/assert.inc
--let $assert_text= 'There is a value 6 in table t1'
--let $assert_cond= [SELECT COUNT(*) AS count FROM t1 WHERE t1.c1 = 6, count, 1] = 1
--source include/assert.inc

--echo
--echo ############################################################
--echo #  7. GTID_EXECUTED on server 2 must contain the transactions
--echo #     from server 3.
--echo #     Validate data on server 2.

--let $rpl_connection_name= server2
--source include/rpl_connection.inc
--let $assert_text= GTID_EXECUTED must contain server 4 GTID_EXECUTED from server 3 with server 1 UUID
--let $assert_cond= [SELECT GTID_SUBSET("$_source_uuid:1-4", @@GLOBAL.GTID_EXECUTED)] = 1
--source include/assert.inc

--let $assert_text= 'There is a value 1 in table t1'
--let $assert_cond= [SELECT COUNT(*) AS count FROM t1 WHERE t1.c1 = 1, count, 1] = 1
--source include/assert.inc
--let $assert_text= 'There is a value 2 in table t1'
--let $assert_cond= [SELECT COUNT(*) AS count FROM t1 WHERE t1.c1 = 2, count, 1] = 1
--source include/assert.inc
--let $assert_text= 'There is a value 6 in table t1'
--let $assert_cond= [SELECT COUNT(*) AS count FROM t1 WHERE t1.c1 = 6, count, 1] = 1
--source include/assert.inc

--echo
--echo ############################################################
--echo #  8. Cleanup

--let $rpl_connection_name= server3
--source include/rpl_connection.inc

DROP TABLE t1;

--let $sync_slave_connection= server1
--let $use_gtids= 0
--source include/sync_slave_sql_with_master.inc
--let $use_gtids= 1
# Stop asynchronous replication connection from server 3

--let $rpl_connection_name= server1
--source include/rpl_connection.inc

--source include/stop_slave.inc

# Restore the GTID_MODE on server 3

--let $rpl_connection_name= server3
--source include/rpl_connection.inc

--let $rpl_gtid_mode= ON
--let $rpl_skip_sync= 1
--let $rpl_server_numbers= 3
--source include/rpl_set_gtid_mode.inc

--let $rpl_connection_name= server1
--source include/rpl_connection.inc

# Stop Group Replication on server 1 and 2 to avoid sync problems

--source include/stop_group_replication.inc

SET session sql_log_bin=0;
CALL mtr.add_suppression("Using ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS implies limitations on the replication topology");
SET session sql_log_bin=1;

--let $allow_rpl_inited=1
--let $_group_replication_local_address= `SELECT @@GLOBAL.group_replication_local_address`
--let $_group_replication_group_seeds= `SELECT @@GLOBAL.group_replication_group_seeds`
--let $restart_parameters=restart:--group_replication_group_name=$group_replication_group_name --group_replication_local_address=$_group_replication_local_address --group_replication_group_seeds=$_group_replication_group_seeds --group_replication_bootstrap_group=1 --group_replication_start_on_boot=OFF
--replace_result $group_replication_group_name GROUP_REPLICATION_GROUP_NAME $_group_replication_local_address GROUP_REPLICATION_LOCAL_ADDRESS $_group_replication_group_seeds GROUP_REPLICATION_GROUP_SEEDS
--source include/restart_mysqld.inc
--let $rpl_server_number= 1
--source include/rpl_reconnect.inc

--let $rpl_connection_name= server2
--source include/rpl_connection.inc
--source include/stop_group_replication.inc

--source include/group_replication_end.inc
