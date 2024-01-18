################################################################################
# Validate that `group_replication_set_as_primary()` does NOT wait for a
# command that does not have any writes before changing the primary.
#
# Test:
#  0. The test uses two servers, M1 and M2.
#  1. Deploy a group in single-primary mode, with server1 being
#     the primary.
#  2. Start a NOP-like statement (for the purposes of these tests,
#     one that does prevent change-primary due to writes, etc.)
#     on the primary, server1. Use the DEBUG_SYNC facility to
#     pause said statement.
#  3. Try to set server2 as primary. The primary change should go
#     right ahead, rather than wait until the NOP is complete.
#  4. Assert that server2 is now the primary, even though
#     the NOP is still active (or rather, "hung").
#  5. Resume the NOP.
#  6. Clean up.
################################################################################
#
# ==== Usage ====
#
# --let $nop_preparation_statement= CREATE TABLE t1 (c1 INT PRIMARY KEY)
# --let $nop_statement= ALTER TABLE t1 ADD COLUMN c2 VARCHAR(3) DEFAULT ''
# --let $nop_cleanup_statement= DROP TABLE t1
# --source ../include/gr_primary_manual_failover_vs_nop.inc
#
#
# Parameters:
#
#   $nop_statement
#     The NOP statement that will be running when the test changes
#     the primary.
#     Mandatory parameter.
#     Example: SHOW STATUS LIKE 'Slow_queries'
#
#   $nop_preparation_statement
#     Optional statement to prepare the scenario.
#
#   $nop_cleanup_statement
#     Optional statement to cleanup the scenario.
#
################################################################################

# Connections:
# server1:    Server1 starts off as primary.
#             Main control: Waits for NOP to start, waits for change-primary
#             to start, shows that server2 becomes primary before NOP finishes.
#             Then lets NOP finish.
# server2:    Becomes primary. We'll show here that that does in fact happen.
# server_1_1: Runs the NOP statement on server1. (And reaps it later.)
# server_1:   Tries to change primary. Must not block until NOP completes.

if (!$nop_statement)
{
  --die "Missing argument 'nop_statement'"
}

--source include/no_view_protocol.inc
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

--let $rpl_connection_name= server2
--source include/connection.inc
--let $server2_uuid= query_get_value(SELECT @@SERVER_UUID, @@SERVER_UUID, 1)
--source include/start_group_replication.inc


--echo
--echo ############################################################
--echo # 2. Start a NOP-like statement on the primary, server1,
--echo #    but do not let it complete yet.
# server_1_1: Send some NOP to server1, and use debug feature to make it "hang".
--let $rpl_connection_name= server_1_1
--source include/connection.inc

if ($nop_preparation_statement)
{
  --echo # The set-up statement for our later test statement:
  --eval $nop_preparation_statement
}

SET DEBUG_SYNC= 'execute_command_before_main_switch SIGNAL nop_running WAIT_FOR nop_resume';
--echo # The statement to test with that shouldn't hold up switching the primary:
--send_eval $nop_statement

# server1: Wait for test statement to complete on server1.
--let $rpl_connection_name= server1
--source include/connection.inc
SET DEBUG_SYNC= 'now WAIT_FOR nop_running';


--echo
--echo ############################################################
--echo # 3. Set server2 as primary.  The primary change should
--echo #    succeed immediately; it should NOT wait until the
--echo #    NOP is complete.
--let $rpl_connection_name= server_1
--source include/connection.inc
--replace_result $server2_uuid SERVER2_UUID
--eval SELECT group_replication_set_as_primary("$server2_uuid");


--echo
--echo ############################################################
--echo # 4. Assert that server2 is now the primary.
--let $rpl_connection_name= server1
--source include/connection.inc

# Show that server2 is now the primary.
--let $assert_text= 'server2 is now the primary'
--let $assert_cond= [SELECT COUNT(*) FROM performance_schema.replication_group_members WHERE member_role="PRIMARY" AND member_id="$server2_uuid"] = 1
--source include/assert.inc

--replace_result $server2_uuid SERVER2_UUID
--source include/gr_assert_secondary_member.inc

--let $rpl_connection_name= server2
--source include/connection.inc
--source include/gr_assert_primary_member.inc


--echo
--echo ############################################################
--echo # 5. Resume the NOP.
--let $rpl_connection_name= server1
--source include/connection.inc
SET DEBUG_SYNC= 'now SIGNAL nop_resume';

--let $rpl_connection_name= server_1_1
--source include/connection.inc
--disable_result_log
--reap
--enable_result_log
SET DEBUG_SYNC= 'RESET';


--echo
--echo ############################################################
--echo # 6. Clean up.
--let $rpl_connection_name= server2
--source include/connection.inc
if ($nop_cleanup_statement)
{
  --echo # The clean-up statement for our earlier test statement:
  --eval $nop_cleanup_statement
  --source include/rpl/sync.inc
}

--let $rpl_connection_name= server1
--source include/connection.inc
--source include/stop_group_replication.inc

--let $rpl_connection_name= server2
--source include/connection.inc
--source include/stop_group_replication.inc

--source include/group_replication_end.inc
