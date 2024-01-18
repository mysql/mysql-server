################################################################################
# Validate that a given statement goes through the `trans_begin` hook on Group
# Replication on server1.
#
# ==== Usage ====
#
# --let $statement_to_hold= STATEMENT
#
#
# Parameters:
#
#   $statement_to_hold
#     The statement to be validated.
#
################################################################################

if (!$statement_to_hold)
{
  --die "Missing argument 'statement_to_hold'"
}

--echo
--echo # Instrument GR to hold the statement on
--echo # `trans_begin` hook on server1.
--let $rpl_connection_name= server_1
--source include/connection.inc
--let $debug_point= group_replication_wait_on_observer_trans
--source include/add_debug_point.inc

--echo
--echo # Execute the statement on server1.
--let $rpl_connection_name= server1
--source include/connection.inc
--send_eval $statement_to_hold

--echo
--echo # Validate that the statement went on
--echo # `trans_begin` hook on server1.
--let $rpl_connection_name= server_1
--source include/connection.inc
SET DEBUG_SYNC= "now WAIT_FOR signal.group_replication_wait_on_observer_trans_waiting";

--echo
--echo # Release the statement from
--echo # `trans_begin` hook on server1.
SET DEBUG_SYNC= 'now SIGNAL signal.group_replication_wait_on_observer_trans_continue';

--echo
--echo # Reap the statement on server1.
--let $rpl_connection_name= server1
--source include/connection.inc
--disable_result_log
--reap
--enable_result_log

--echo
--echo # Remove GR instrumentaion to hold the statement on
--echo # `trans_begin` hook on server1.
--let $rpl_connection_name= server_1
--source include/connection.inc
--let $debug_point= group_replication_wait_on_observer_trans
--source include/remove_debug_point.inc
