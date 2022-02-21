# ==== Purpose ====
#
# Check commands run successfully with change of primary operation.
# This file may not be used for multi-statements or explicit commands like
# START TRANSACTION;
# ==== Usage ====
#
# --let $conn_of_new_primary= CLIENT_CONNECTION
# --let $command_to_execute= COMMAND
# --let $conn_for_command= CLIENT_CONNECTION
# --let $conn_for_debug_sync= CLIENT_CONNECTION
# --let $conn_for_primary= CLIENT_CONNECTION
# [--let $connection_is_killed= 1]
# --source ../include/gr_parallel_command_primary_change.inc
#
# Parameters:
#
#   $conn_of_new_primary
#     Connection of member which is to be selected as primary.
#     Required to fetch SERVER_UUID.
#
#   $command_to_execute
#     Command to run before running UDF group_replication_set_as_primary.
#     Command will be blocked post which UDF will execute.
#
#   $conn_for_command
#     Connection on which to execute the command.
#
#   $conn_for_debug_sync
#     Connection on which DEBUG_SYNC should be executed.
#
#   $conn_for_primary
#     Connection on which UDF group_replication_set_as_primary should be
#     executed.
#
#   $connection_is_killed
#     Set to 1, if connection will be killed by UDF.

if (!$conn_of_new_primary)
{
  --die "Missing parameter 'conn_of_new_primary'"
}

if (!$command_to_execute)
{
  --die "Missing parameter 'command_to_execute'"
}

if (!$conn_for_command)
{
  --die "Missing parameter 'conn_for_command'"
}

if (!$conn_for_debug_sync)
{
  --die "Missing parameter 'conn_for_debug_sync'"
}

if (!$conn_for_primary)
{
  --die "Missing parameter 'conn_for_primary'"
}

--let $_saved_connection= $CURRENT_CONNECTION

--let $rpl_connection_name= $conn_of_new_primary
--source include/rpl_connection.inc
--let $uuid_of_new_primary= query_get_value(SELECT @@SERVER_UUID, @@SERVER_UUID, 1)

--echo
--echo # Start a transaction on $conn_for_command and block it.

--let $rpl_connection_name= $conn_for_command
--source include/rpl_connection.inc

SET DEBUG_SYNC="before_reset_query_plan SIGNAL before_reset_query_ongoing WAIT_FOR after_reset_query_finished";

--send_eval $command_to_execute;

--let $rpl_connection_name= $conn_for_debug_sync
--source include/rpl_connection.inc

SET DEBUG_SYNC="now WAIT_FOR before_reset_query_ongoing";

--echo
--echo # Begin change of primary on $conn_for_primary.

--let $rpl_connection_name= $conn_for_primary
--source include/rpl_connection.inc

--replace_result $uuid_of_new_primary MEMBER_UUID
--eval SELECT group_replication_set_as_primary("$uuid_of_new_primary", 1)

--echo
--echo # Assert primary changed.

--let $rpl_connection_name= $conn_of_new_primary
--source include/rpl_connection.inc

--source include/gr_assert_primary_member.inc

--echo
--echo # Reap the transaction.

--let $rpl_connection_name= $conn_for_debug_sync
--source include/rpl_connection.inc
# before_reset_query_plan is present at 2 places so for safety added 2 signals
SET DEBUG_SYNC="now SIGNAL after_reset_query_finished";
SET DEBUG_SYNC="now SIGNAL after_reset_query_finished";

--let $rpl_connection_name= $conn_for_command
--source include/rpl_connection.inc

if ($connection_is_killed)
{
  --error CR_SERVER_LOST
  --reap

  --enable_reconnect
  --source include/wait_until_connected_again.inc
  --disable_reconnect
}
if (!$connection_is_killed)
{
  --reap
}
SET DEBUG_SYNC="RESET";

--connection $_saved_connection
--let $uuid_of_new_primary=
--let $conn_of_new_primary=
--let $command_to_execute=
--let $conn_for_command=
--let $conn_for_debug_sync=
--let $conn_for_primary=
--let $connection_is_killed=
--let $_saved_connection=
