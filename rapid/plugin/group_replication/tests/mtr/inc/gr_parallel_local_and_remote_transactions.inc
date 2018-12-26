###############################################################################
# This include file is created to test local transaction's behaviour
# (conflict/success) against a transaction that is ordered first
# on a remote server and broadcasted to local server.
#
# 1. Set a debug sync before broadcast message to group on local server.
# 2. Commit a transaction (called local transaction) that will be blocked
#    before broadcast.
# 3. Wait until local server reaches the debug sync point.
# 4. Execute a transaction on remote server (called remote transaction), that
#    will reach first certification, since local transaction is blocked before
#    broadcast.
# 5. Signal the waiting thread on local server to resume.
# 6. Wait for remote transaction to be executed succesfully on local server.
# 7. Check the local transaction behaviour (depends on the scenario, it should be
#    rolled back (conflict scenario) or it should succesfully commit(success scenario).
# 8. Sync everything.
# 9. Assert that number of certified transactions are the expected ones.
#10. Assert that GTID is increased as expected.
#11. Cleanup (restore the connection back to original)
#
# ##### Usage #######
#
# --let $local_server_connection1=server_1
# --let $local_server_connection2=server1
# --let $remote_server_connection=server2
# --let $local_transaction= INSERT INTO t2 VALUES (5); INSERT INTO t1 VALUES (5);
# --let $remote_transaction=UPDATE t1 SET c1=6 WHERE c1=2
# --let $conflict_test=1
# --source ../inc/gr_parallel_local_and_remote_transactions.inc
#
# ##### Parameters #####
#
#  $local_server_connection1
#      Connection name on which the $local_transaction needs to be executed.
#
#  $local_server_connection2
#      Connection name on which debug sync commands can be executed to block
#      local transaction before broadcasting.
#
#  $remote_server_connection
#      Connection name on which the $remote_transaction needs to be executed.
#
#  $local_transaction
#       The transaction statements that needs to be executed on local server.
#       If there are more than one statement, they should be specified in the same
#       line with semicolon(;) as delimiter.
#       Eg: --let $local_transaction= INSERT INTO t2 VALUES (5); INSERT INTO t1 VALUES (5);
#
#  $remote_transaction
#       The transaction statements that needs to be executed on remote server.
#       If there are more than one statement, they should be specified in the same
#       line with semicolon(;) as delimiter.
#       Eg: --let $remote_transaction= INSERT INTO t2 VALUES (5); INSERT INTO t1 VALUES (5);
#
#  $conflict_test
#       0 or 1 depends on the testing scenario.
#       If the test case is conflict scenario(1), local transaction
#       should end up in an error stating that it was aborted,
#       since transactions are conflicting and transaction on
#       remote server was ordered first.
#       If the test case is is positive scenario(0),
#       no error will be seen while executing local transaction and
#       should be committed succesfully.
#
#  P.S: This .inc will restore the original connection back.
#
--echo
--echo ############################################################
--echo # 0. Initial setup and checks.

-- let $old_conn= $CURRENT_CONNECTION
if (!$local_server_connection1)
{
  --die !!!ERROR IN TEST: you must set $local_server_connection1 on which the local_transaction needs to be executed.
}
if (!$local_server_connection2)
{
  --die !!!ERROR IN TEST: you must set $local_server_connection2 on which debug sync needs to be set to block the broadcast.
}
if (!$remote_server_connection)
{
  --die !!!ERROR IN TEST: you must set $remote_server_connection on which the remote_transaction needs to be executed.
}
if (!$local_transaction)
{
  --die !!!ERROR IN TEST: you must set $local_transaction which will be executed on local_server_connection1.
}
if (!$remote_transaction)
{
  --die !!!ERROR IN TEST: you must set $remote_transaction which will be executed on remote_server_connection.
}

--let $rpl_connection_name= $local_server_connection1
--source include/rpl_connection.inc

# Include gtid_utils if it is not included already
if (!$gtid_util_included)
{
  SET SESSION sql_log_bin= 0;
  --source include/gtid_utils.inc
  SET SESSION sql_log_bin= 1;
  --let $gtid_utils_installed= 1;
}
# Remember the gtid position
--source include/gtid_step_reset.inc
--let $prev_certified_transactions= query_get_value(SELECT Count_Transactions_Checked from performance_schema.replication_group_member_stats, Count_Transactions_Checked, 1)
--let $prev_negatively_certified= query_get_value(SELECT Count_Conflicts_Detected from performance_schema.replication_group_member_stats, Count_Conflicts_Detected, 1)

--echo
--echo ############################################################
--echo # 1. Set a debug sync before broadcast message to group on
--echo #    connection local_server_connection1.
--let $rpl_connection_name= $local_server_connection1
--source include/rpl_connection.inc
SET @debug_save= @@GLOBAL.DEBUG;
SET @@GLOBAL.DEBUG='d,group_replication_before_message_broadcast';

--echo
--echo #####################################################################
--echo # 2. Commit local_transaction that will be blocked before broadcast.
BEGIN;
--eval $local_transaction
--send COMMIT

--echo
--echo ############################################################
--echo # 3. Wait until local transaction reaches the
--echo # group_replication_before_message_broadcast debug sync point.
--let $rpl_connection_name= $local_server_connection2
--source include/rpl_connection.inc
--let $wait_condition=SELECT COUNT(*)=1 FROM INFORMATION_SCHEMA.PROCESSLIST WHERE State = 'debug sync point: now'
--source include/wait_condition.inc

--echo
--echo ############################################################
--echo # 4. Execute a transaction on remote server, that will reach first
--echo #    certification, since transaction on the local server
--echo #    is blocked before broadcast.
--let $rpl_connection_name= $remote_server_connection
--source include/rpl_connection.inc
BEGIN;
--eval $remote_transaction
COMMIT;

--echo
--echo ############################################################
--echo # 5. Signal the waiting thread on local server to resume the
--echo #    transaction.
--let $rpl_connection_name= $local_server_connection2
--source include/rpl_connection.inc
SET DEBUG_SYNC='now SIGNAL waiting';
SET @@GLOBAL.DEBUG= @debug_save;


--echo
--echo ############################################################
--echo # 6. Wait for remote transaction to be executed succesfully
--echo      on local server.
--let $rpl_connection_name= $remote_server_connection
--source include/rpl_connection.inc
--let $sync_slave_connection= $local_server_connection2
--source include/sync_slave_sql_with_master.inc

--echo ############################################################
--echo # 7. If the test case is conflict scenario, local transaction
--echo #    will end up in an error stating that it was aborted,
--echo #    since transactions are conflicting and transaction on
--echo #    remote server was ordered first. If the test case is
--echo #    is positive scenario, no error will be seen here.
--let $rpl_connection_name= $local_server_connection1
--source include/rpl_connection.inc
if ($conflict_test)
{
  --error ER_TRANSACTION_ROLLBACK_DURING_COMMIT
}
--reap

--let $rpl_connection_name= $local_server_connection1
--source include/rpl_connection.inc
SET @@GLOBAL.DEBUG=@debug_save;

--echo ############################################################
--echo # 8. Sync everything
--source include/rpl_sync.inc

--echo ############################################################
--echo # 9. Assert that number of certified transactions are the
--echo #    expected ones.
--let $expected_certified_transactions= `SELECT $prev_certified_transactions + 2`
--let $certified_transactions= query_get_value(SELECT Count_Transactions_Checked from performance_schema.replication_group_member_stats, Count_Transactions_Checked, 1)
--let $assert_text= The value of Count_Transactions_Checked should be $expected_certified_transactions after starting group replication
--let $assert_cond= "$certified_transactions" = $expected_certified_transactions
--source include/assert.inc

--let $expected_negatively_certified_transactions= `SELECT $prev_negatively_certified + $conflict_test`
--let $negatively_certified= query_get_value(SELECT Count_Conflicts_Detected from performance_schema.replication_group_member_stats, Count_Conflicts_Detected, 1)
--let $assert_text= The value of Count_Conflicts_Detected should be $expected_negatively_certified_transactions after starting group replication
--let $assert_cond= "$negatively_certified" = $expected_negatively_certified_transactions
--source include/assert.inc

--echo ############################################################
--echo # 10. Assert that GTID is increased as expected
--let $gtid_step_count= `SELECT 2 - $conflict_test`
--let $gtid_step_uuid=`SELECT @@global.group_replication_group_name`
--let $gtid_step_only_count= 1
--source include/gtid_step_assert.inc

# uninstall gtid_utils if it is installed by this file.
if ($gtid_utils_installed)
{
  --let $rpl_connection_name= $local_server_connection1
  --source include/rpl_connection.inc
  SET SESSION sql_log_bin= 0;
  --source include/gtid_utils_end.inc
  SET SESSION sql_log_bin= 1;
}

--echo ############################################################
--echo # 11. Cleanup (restore the connection back to original)
--let $rpl_connection_name= $old_conn
--source include/rpl_connection.inc
