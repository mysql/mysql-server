################################################################################
# Validate that `group_replication_set_as_primary()` does wait for a ongoing
# DDL statement to complete before actually changing the primary.
#
# Test:
#  0. The test uses two servers, M1 and M2.
#  1. Deploy a group in single-primary mode, with server1 being
#     the primary.
#  2. Start a DDL statement on the primary, server1.
#     Use the DEBUG_SYNC facility to pause said statement.
#  3. Try to set server2 as primary. The primary change should
#     wait until the DDL statement finishes.
#  4. Assert that server1 continues to be the primary (as
#     changing the primary must wait for the pending DDL to
#     finish first).
#  5. Resume the DDL.
#  6. Changing the primary should now go ahead; server2 should
#     become primary.
#  7. Validate that the DDL statement was applied on server2
#     through the 'group_replication_applier' channel.
#  8. Clean up.
################################################################################
#
# ==== Usage ====
#
# --let $ddl_preparation_statement= CREATE TABLE t1 (c1 INT PRIMARY KEY)
# --let $ddl_statement= ALTER TABLE t1 ADD COLUMN c2 VARCHAR(3) DEFAULT ''
# --let $ddl_cleanup_statement= DROP TABLE t1
# --source ../include/gr_primary_manual_failover_vs_ddl.inc
#
#
# Parameters:
#
#   $ddl_statement
#     The DDL statement to run while the test tries to change the primary.
#     Mandatory parameter.
#
#   $ddl_preparation_statement
#     Optional DDL statement to prepare the scenario.
#     Example: CREATE TABLE ...
#
#   $ddl_cleanup_statement
#     Optional DDL statement to clean up the scenario.
#     Example: DROP TABLE ...
#
################################################################################
#
# Connections
#
# server1:    Server1 starts off as primary.
#             Main control: Waits for DDL to start, waits for change-primary
#             to start, shows that server1 remains primary until DDL finishes.
#             Then lets DDL finish.
# server2:    Becomes primary. We'll show here that that does in fact happen,
#             and that the DDL statement is replicated here beforehand.
# server_1_1: Runs the DDL statement on server1. (And reaps it later.)
# server_1:   Tries to change primary. Must block until DDL completes.
#
################################################################################

if (!$ddl_statement)
{
  --die "Missing argument 'ddl_statement'"
}

--source include/no_cursor_protocol.inc
--source include/have_debug_sync.inc
--source include/have_group_replication_plugin.inc
--let $rpl_skip_group_replication_start= 1
--let $rpl_group_replication_single_primary_mode= 1
--source include/group_replication.inc


--echo
--echo ############################################################
--echo # 1. Deploy a group in single-primary mode.  server1 is the
--echo #    primary.
--let $rpl_connection_name= server1
--source include/connection.inc
--let $server1_uuid= query_get_value(SELECT @@SERVER_UUID, @@SERVER_UUID, 1)
--source include/start_and_bootstrap_group_replication.inc
if ($ddl_preparation_statement)
{
  --echo # The set-up statement for our later test statement:
  --eval $ddl_preparation_statement
}

--let $rpl_connection_name= server2
--source include/connection.inc
--let $server2_uuid= query_get_value(SELECT @@SERVER_UUID, @@SERVER_UUID, 1)
--source include/start_group_replication.inc


--echo
--echo ############################################################
--echo # 2. Start a DDL statement on server1, but do not let it complete yet.
# server_1_1: Send some DDL to server1, and use debug feature to make it "hang".
--let $rpl_connection_name= server_1_1
--source include/connection.inc

if ($ddl_transaction_statement)
{
  --echo # Start transaction for our test statement:
  --eval $ddl_transaction_statement
}

SET DEBUG_SYNC= 'execute_command_before_main_switch SIGNAL ddl_running WAIT_FOR ddl_resume';
--echo # The statement to test with that should hold up switching the primary:
--send_eval $ddl_statement

# server1: Wait for DDL to complete on server1.
--let $rpl_connection_name= server1
--source include/connection.inc
SET DEBUG_SYNC= 'now WAIT_FOR ddl_running';


--echo
--echo ############################################################
--echo # 3. Set server2 as primary. The primary change must wait
--echo #    until the DDL statement has completed.
--let $rpl_connection_name= server_1
--source include/connection.inc
--replace_result $server2_uuid SERVER2_UUID
--send_eval SELECT group_replication_set_as_primary("$server2_uuid")


--echo
--echo ############################################################
--echo # 4. Assert that server1 continues to be the primary, as the
--echo #    primary-change can not go ahead until the DDL statement
--echo #    finishes. (And that statement of course can not finish
--echo #    yet as it is hanging on DEBUG_SYNC.)
--let $rpl_connection_name= server1
--source include/connection.inc

# Wait for the primary-change to begin.
--let $wait_condition=SELECT COUNT(*)=1 FROM performance_schema.events_stages_current WHERE event_name LIKE "stage/group_rpl/Primary Switch: waiting for pending transactions to finish"
--source include/wait_condition.inc

# Show that primary hasn't actually changed yet -- it is still server1.
--let $assert_text= 'server1 continue to be primary'
--let $assert_cond= [SELECT COUNT(*) FROM performance_schema.replication_group_members WHERE member_role="PRIMARY" AND member_id="$server1_uuid"] = 1
--source include/assert.inc


--echo
--echo ############################################################
--echo # 5. Let the DDL statement finish.
--let $rpl_connection_name= server1
--source include/connection.inc
SET DEBUG_SYNC= 'now SIGNAL ddl_resume';

# server_1_1: reap DDL statement once it completes.
--let $rpl_connection_name= server_1_1
--source include/connection.inc

--reap

SET DEBUG_SYNC= 'RESET';

--echo
--echo ############################################################
--echo # 6. server2 should now be primary.

# server_1: reap "change primary" when it completes.
--let $rpl_connection_name= server_1
--source include/connection.inc
--replace_result $server2_uuid SERVER2_UUID
--reap
--source include/gr_assert_secondary_member.inc

# server2: prove that this is now the primary.
--let $rpl_connection_name= server2
--source include/connection.inc
--source include/gr_assert_primary_member.inc


--echo
--echo ############################################################
--echo # 7. Validate that the DDL was applied on server2 through
--echo #    the 'group_replication_applier' channel.

--let $rpl_connection_name= server2
--source include/connection.inc

--let $rpl_channel_name= 'group_replication_applier'
--let $binlog_file= server-relay-log-group_replication_applier.000002
--source include/rpl/deprecated/show_relaylog_events.inc

--let $rpl_channel_name=


--echo
--echo ############################################################
--echo # 8. Clean up.

# Run clean-up statement if caller defined one.
--let $rpl_connection_name= server2
--source include/connection.inc
if ($ddl_cleanup_statement)
{
  --echo # The clean-up statement for our earlier test statement:
  --eval $ddl_cleanup_statement
  --source include/rpl/sync.inc
}

# Stop replication.
--let $rpl_connection_name= server1
--source include/connection.inc
--source include/stop_group_replication.inc

--let $rpl_connection_name= server2
--source include/connection.inc
--source include/stop_group_replication.inc

--source include/group_replication_end.inc

# The End.
