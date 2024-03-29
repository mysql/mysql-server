###############################################################
# The purpose of this test is to exercise all possible options
# that avoid a node connecting to himself as seed, if a
# misconfiguration accidentally happens, when using names.
#
#
# The following combinations (as an example) must succeed:
#
# boot_node = off
# local_address = name:12345
# peers = raw_address:12345,127.0.0.1:12346
#
# boot_node = off
# local_address = raw_address:12345
# peers = name:12345,127.0.0.1:12346
#
#
# For that one needs to:
# - Skip start as a whole
# - Start server 1
# - Build scenario 1
# - Start server 2
# - Make sure it comes online
# - Build and run scenario 2
# - Cleanup
#
# ==== Usage ====
#
# --let $localhost_address= (any raw address in IPv4 or v6 format)
# --source ../include/gr_startup_check_node_seed_skips_own_address.inc
#
###############################################################

if (!$localhost_address)
{
  --die "Missing argument $localhost_address"
}

--let $group_replication_group_name= 89ab83b0-9f17-11e3-a5e2-0800200c9a66

--source include/have_group_replication_plugin.inc
--let $rpl_skip_group_replication_start= 1
--source include/group_replication.inc

# Start server 1 by hand
--connection server1
--echo server1
--source include/start_and_bootstrap_group_replication.inc

# Go to server 2
--connection server2
--echo server2

# Backup the original values
--let $original_server_2_local_address= `SELECT @@GLOBAL.group_replication_local_address`
--let $original_server_2_seeds= `SELECT @@GLOBAL.group_replication_group_seeds`

let $have_xcom_stack= `SELECT @@group_replication_communication_stack LIKE 'XCOM'`;
--let $_rpl_gcs_server_port=
if ($have_xcom_stack)
{
  --let $_rpl_gcs_server_port= $SERVER_GR_PORT_2
}
if (!$have_xcom_stack)
{
  --let $_rpl_gcs_server_port= $SERVER_MYPORT_2
}


--echo "1. Test local address with raw address and seeds with name"

# Assemble the new values:
# - local address with raw values
# - local address first in seeds with name (which is default)
--let $localhost_address= [::1]:
--let $local_address_with_raw_address= $localhost_address$_rpl_gcs_server_port

# Set values
--disable_query_log
--eval SET GLOBAL group_replication_local_address= "$local_address_with_raw_address"
--eval SET GLOBAL group_replication_group_seeds= "$original_server_2_local_address,$original_server_2_seeds"
--enable_query_log

# Start the server and wait for successful start
--source include/start_group_replication.inc

--let $group_replication_member_state= ONLINE
--source include/gr_wait_for_member_state.inc

# Stop the server
--source include/stop_group_replication.inc

--echo "2. Test local address with name address and seeds with raw address"

# Assemble the new values:
# - local address with name values
# - local address first in seeds with raw value

--disable_query_log
--eval SET GLOBAL group_replication_local_address= "$original_server_2_local_address"
--eval SET GLOBAL group_replication_group_seeds= "$local_address_with_raw_address,$original_server_2_seeds"
--enable_query_log

--source include/start_group_replication.inc

# Start the server and wait for successful start

--let $group_replication_member_state= ONLINE
--source include/gr_wait_for_member_state.inc

# Stop the server
--source include/stop_group_replication.inc

--echo "3. Test local address with name address and seeds with name address"

# Assemble the new values:
# - local address with name values
# - local address first in seeds with name value

--disable_query_log
--eval SET GLOBAL group_replication_local_address= "$original_server_2_local_address"
--eval SET GLOBAL group_replication_group_seeds= "$original_server_2_local_address,$original_server_2_seeds"
--enable_query_log

--source include/start_group_replication.inc

# Start the server and wait for successful start

--let $group_replication_member_state= ONLINE
--source include/gr_wait_for_member_state.inc

# Cleanup

--disable_query_log
--eval SET GLOBAL group_replication_local_address= "$original_server_2_local_address"
--eval SET GLOBAL group_replication_group_seeds= "$original_server_2_seeds"
--enable_query_log

--source include/group_replication_end.inc
