# ==== Purpose ====
#
# The purpose of this script is to test if when correctly configured we
# can clone a replica and it will start replicating on server start.
#
# ==== Usage ====
#
# This script uses no parameters, for its uses you can just include it.
#
#--source common/rpl/replica_start_after_clone.inc
#
# You can this point do extra validations.
# To clean the test setup you should invoke
#
#--source common/rpl/replica_start_after_clone_cleanup.inc
#
# ==== Setup ====
#
# The scrip will configure server 3 to clone server 2 that is a replica
# of server 1.
# It then validates server 3 is replicating from server 1
#
# ==== Implementation ====
#
# 1. Create connections to server3
# 2. Create a table for testing
# 3. Install clone on server 2
#    Configure server 3 to invoke clone
# 4. Execute a transaction on server 1, check 3 received it
#
# ==== References ====
#
# Bug#29995256: PRIVILEGE_CHECKS_USER NOT CLONED FOR RECOVERY CHANNEL
# WL#7491: GTID-based replication applier recovery and positioning
#


--echo #
--echo # 1. Create connections to server3

--connect(server_3,127.0.0.1,root,,test,$SERVER_MYPORT_3,)
--connect(server_3_1,127.0.0.1,root,,test,$SERVER_MYPORT_3,)

--echo #
--echo # 2. Create a table for testing

--let $rpl_connection_name= server_1
--source include/connection.inc

CREATE TABLE test.t1 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;

--source include/rpl/sync_to_replica.inc

--echo #
--echo # 3. Install clone on server 2
--echo #    Configure server 3 to invoke clone

--let $rpl_connection_name= server_2
--source include/connection.inc

--replace_result $CLONE_PLUGIN CLONE_PLUGIN
--eval INSTALL PLUGIN clone SONAME '$CLONE_PLUGIN'

--let $rpl_connection_name= server_3
--source include/connection.inc

# Make server 3 run with mysqld_safe
--source include/spawn_monitoring_process.inc

# Install clone
--replace_result $CLONE_PLUGIN CLONE_PLUGIN
--eval INSTALL PLUGIN clone SONAME '$CLONE_PLUGIN'

# Set the clone valid donor list to include server 2
--replace_result $SERVER_MYPORT_2 SERVER2_PORT
--eval SET GLOBAL clone_valid_donor_list = 'localhost:$SERVER_MYPORT_2'

# Clone server 3 from server 2
--replace_result $SERVER_MYPORT_2 SERVER2_PORT
--eval CLONE INSTANCE FROM root@localhost:$SERVER_MYPORT_2 IDENTIFIED BY ''
--source include/wait_until_disconnected.inc

--let $rpl_server_number= 3
--source include/rpl/reconnect.inc

--echo #
--echo # 4. Execute a transaction on server 1, check 3 received it

--let $rpl_connection_name= server_1
--source include/connection.inc

INSERT INTO t1 VALUES (1);

--let $sync_slave_connection= server_3
--source include/rpl/sync_to_replica.inc

--let $rpl_connection_name= server_3
--source include/connection.inc

--let $server_1_data = `SELECT COUNT(*) FROM test.t1`
--let $assert_text= All info from server 1 is present
--let $assert_cond= $server_1_data = 1
--source include/assert.inc
