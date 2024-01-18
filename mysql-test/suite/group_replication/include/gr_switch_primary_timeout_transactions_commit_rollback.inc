# ==== Purpose ====
#
# Validates UDF group_replication_set_as_primary and confirms that connection
# running a transactions that did not reach commit stage is disconnected when
# UDF group_replication_set_as_primary is executed.
# UDF group_replication_set_as_primary runs successfully and primary is changed.
#
# NOTES:
# 1. Caller should make sure server1 is primary and server2 is secondary.
# 2. Connections used: server1, server_1, server2
#
# ==== Usage ====
#
# --let $statement_to_execute_on_server1= STATEMENT1
# --let $statement_to_execute_on_server2= STATEMENT2
# --source ../include/gr_switch_primary_timeout_transactions_commit_rollback.inc
#
# Parameters:
#
#   $statement_to_execute_on_server1
#     A statement to execute when UDF is invoked from SECONDARY.
#
#   $statement_to_execute_on_server2
#     A statement to execute when UDF is invoked from PRIMARY.


################################################################################
########## UDF called from SECONDARY ##########
################################################################################

--echo
--echo # Block all commits on server1 and initiate a transaction.
--echo # Begin change of primary from server2(secondary).

--let $rpl_connection_name= server1
--source include/connection.inc

--source include/gr_assert_primary_member.inc

SET @@GLOBAL.DEBUG= '+d,trans_delegate_before_commit_before_before_call_observers';

--send_eval $statement_to_execute_on_server1

--let $rpl_connection_name= server_1
--source include/connection.inc

SET DEBUG_SYNC= "now WAIT_FOR signal.trans_delegate_before_commit_before_before_call_observers_reached";
SET @@GLOBAL.DEBUG= '-d,trans_delegate_before_commit_before_before_call_observers';

--let $rpl_connection_name= server2
--source include/connection.inc

--source include/gr_assert_secondary_member.inc

--replace_result $server2_uuid SERVER2_UUID
--eval SELECT group_replication_set_as_primary("$server2_uuid", 0);

################################################################################
--echo
--echo # Unblock transactions on server1.
--echo # Transaction should fail it was not committing so it was killed
--echo # and client connection dropped.

--let $rpl_connection_name= server_1
--source include/connection.inc

SET DEBUG_SYNC= "now SIGNAL signal.trans_delegate_before_commit_before_before_call_observers_waiting";

--let $rpl_connection_name= server1
--source include/connection.inc
--error CR_SERVER_LOST
--reap

--source include/wait_until_connected_again.inc

SET DEBUG_SYNC= 'RESET';

################################################################################
--echo
--echo # Assert primary changed.

--let $rpl_connection_name= server2
--source include/connection.inc

--source include/gr_assert_primary_member.inc

--let $rpl_connection_name= server1
--source include/connection.inc

--source include/gr_assert_secondary_member.inc

################################################################################
########## UDF called from PRIMARY ##########
################################################################################

--echo
--echo # Block all commits on server2 and initiate a transaction.
--echo # Begin change of primary from server2(primary).

--let $rpl_connection_name= server2
--source include/connection.inc

SET @@GLOBAL.DEBUG= '+d,trans_delegate_before_commit_before_before_call_observers';

--send_eval $statement_to_execute_on_server2

--let $rpl_connection_name= server_2
--source include/connection.inc

SET DEBUG_SYNC= "now WAIT_FOR signal.trans_delegate_before_commit_before_before_call_observers_reached";
SET @@GLOBAL.DEBUG= '-d,trans_delegate_before_commit_before_before_call_observers';

--replace_result $server1_uuid SERVER1_UUID
--eval SELECT group_replication_set_as_primary("$server1_uuid", 2);

################################################################################
--echo
--echo # Unblock transactions on server2.
--echo # Transaction should fail it was not committing so it was killed
--echo # and client connection dropped.

SET DEBUG_SYNC= "now SIGNAL signal.trans_delegate_before_commit_before_before_call_observers_waiting";

--let $rpl_connection_name= server2
--source include/connection.inc

--error CR_SERVER_LOST
--reap

--source include/wait_until_connected_again.inc

SET DEBUG_SYNC= 'RESET';

################################################################################
--echo
--echo # Assert primary changed.

--let $rpl_connection_name= server1
--source include/connection.inc

--source include/gr_assert_primary_member.inc

--let $rpl_connection_name= server2
--source include/connection.inc

--source include/gr_assert_secondary_member.inc
