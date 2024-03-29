# ==== Purpose ====
#
# This test's goal is to check that no deadlock occurs in an SPCO enabled
# multi-threaded replica when client connections issue global locking or
# ACL statements concurrently with the applier worker threads.
#
# ==== Usage ====
#
# [--let $rpl_diff_statemetn = <STATEMENT>]
#  The statement to be used with `include/rpl_diff.inc`. This parameter is
#  mandatory.
#
# [--let $mts_spco_gd_trx_blocking_worker_1 = <STATEMENT>]
#  The statement to be executed by the first available worker that will be
#  blocked until all workers are waiting on the commit order.
#
# [--let $mts_spco_gd_trx_assigned_worker_2 = <STATEMENT>]
#  The statement to be execute by one of the workers that should block the
#  client connection.
#
# [--let $mts_spco_gd_trx_assigned_worker_3 = <STATEMENT>]
#  The statement to be execute by one of the workers that should block the
#  client connection.
#
# [--let $mts_spco_gd_worker_3_only_runs_after_deadlock = 0|1]
#  Whether or not worker 3 should only proceed with the transaction
#  execution after the deadlocak has been detected. If `0`, worker 3 will
#  wait on the commit order queue before the deadlock is detected, leading
#  to the use-case where both workers 2 and 3 will be force to rollback by
#  the MDL graph infra-structure. If `1`, worker 3 will only start the
#  transaction execution after the deadlock is detected and the coordinator
#  already started the shutdown process, leading to the use-case where
#  worker 2 is forced to rollback by the MDL graph infra-structure but
#  worker 3 will rollback due to the commit queue infra-structure. These
#  two use-cases generate a different set of error messages, hence the need
#  to test each of them separately, and prohibit executing a scenario where
#  either of them can happen nondeterministically.
#
# [--let $mts_spco_gd_trx_blocking_client = <STATEMENT>]
#  The statement to be executed by a client connection that should be
#  blocked by one or both workers waiting on the commit queue.
#
# [--let $mts_spco_gd_state_blocking_client = <STATUS text>]
#  The status text for the client connection when it becomes blocked by one
#  of the workers, as appears in information_schema.processlist.STATE
#
# [--let $mts_spco_gd_error_expected_replica = <ERROR>]
#  The error that should be expected the replica applier to reach in order
#  to not reach the deadlock, if any.
#
# [--let $mts_spco_gd_wait_for_coordinator_running_state = <STATUS text>]
#  The status text to wait for the coordinator to reach before letting it
#  stop, if any.
#
# [--let $mts_spco_gd_trx_to_client_unblocking_workers = <STATEMENT>]
#  The statement the client connection should execute after the replica
#  applier thread stopped, in order for the workers to finish applying
#  after restarted.
#
# [--let $mts_spco_gd_trx_finishing_group = <STATEMENT>]
#  The statement to be executed on the source to allow the replica to
#  properly finish synchronizing with the master.
#
# ==== Implementation ====
#
#   1. Execute $mts_spco_gd_trx_blocking_worker_1,
#      $mts_spco_gd_trx_assigned_worker_2 and
#      $mts_spco_gd_trx_assigned_worker_3 on the source in a way that they
#      will belong to the same commit group and scheduled to be
#      parallelized on the replica.
#   2. On the replica, using client connection A, start a transaction and
#      assign it the same GTID as $mts_spco_gd_trx_blocking_worker_1.
#   3. Start the replication threads
#   4. On the replica, ensure that the applier worker threads are waiting
#      on the pending client connection transaction.
#   5. On the replica, using client connection B, execute
#      $mts_spco_gd_trx_blocking_client (to block the first replicated
#      transaction further down the execution), leading to the following
#      lock acquisition dependencies: Client B --statement required lock-->
#      Worker 2 --commit order lock--> Worker 1 --gtid lock--> Client A.
#   6. On the replica, ensure that client connection B is waiting on the
#      lock being held by Worker 2 or Worker 3, reaching the state
#      $mts_spco_gd_state_blocking_client.
#   7. On the replica, using client connection A, rollback the pending
#      transaction, leading to the following lock acquisition dependencies:
#      Client B --statement required lock--> Worker 2 --commit order
#      lock--> Worker 1 --statement required lock--> Client B.
#   8. Wait for the client connection B to receive its return packet from
#      the server.
#   9. Wait for the coordinator to reach the
#      $mts_spco_gd_wait_for_coordinator_running_state state before
#      allowing it to error out, if any is defined.
#  10. Wait for the replica applier thread to error out with
#      $mts_spco_gd_error_expected_replica, if any is defined.
#  11. Synchronize the replica with the source.
#
# ==== References ====
#
# WL#13574 Include MDL and ACL locks in MTS deadlock detection
#          infra-structure
#

--source include/have_debug_sync.inc
--source include/have_slave_repository_type_table.inc

if ($mts_spco_gd_trx_blocking_worker_1 == '')
{
  --die ERROR IN TEST: you must set `mts_spco_gd_trx_blocking_worker_1` before sourcing mts_spco_generate_deadlock.test
}
if ($mts_spco_gd_error_expected_replica == '')
{
  --let $mts_spco_gd_error_expected_replica = 0
}
if ($mts_spco_gd_worker_3_only_runs_after_deadlock == '')
{
  --let $mts_spco_gd_worker_3_only_runs_after_deadlock = 0
}

--source include/rpl_connection_slave.inc
--let $mts_spco_gd_is_replica_sql_running = query_get_value(SHOW REPLICA STATUS, Replica_SQL_Running, 1)
if ($mts_spco_gd_is_replica_sql_running == 'Yes')
{
  --die ERROR IN TEST: slave applier thread must be stopped before sourcing mts_spco_generate_deadlock.test
}

if ($mts_spco_gd_seqno_to_wait_for == '')
{
  --let $mts_spco_gd_seqno_to_wait_for = 0
}

--inc $mts_spco_gd_seqno_to_wait_for
--let $mts_spco_gd_gtid_to_wait_for_1 = "aaaaaaaa-1111-bbbb-2222-cccccccccccc:$mts_spco_gd_seqno_to_wait_for"

#   1. Execute $mts_spco_gd_trx_blocking_worker_1,
#      $mts_spco_gd_trx_assigned_worker_2 and
#      $mts_spco_gd_trx_assigned_worker_3 on the source in a way that they
#      will belong to the same commit group and scheduled to be
#      parallelized on the replica.
--source include/rpl_connection_master.inc
--let $debug_point = set_commit_parent_100
--source include/add_debug_point.inc

--eval SET GTID_NEXT = $mts_spco_gd_gtid_to_wait_for_1
--eval $mts_spco_gd_trx_blocking_worker_1
SET GTID_NEXT = AUTOMATIC;

if ($mts_spco_gd_trx_assigned_worker_2 != '')
{
  --inc $mts_spco_gd_seqno_to_wait_for
  --let $mts_spco_gd_gtid_to_wait_for_2 = "aaaaaaaa-1111-bbbb-2222-cccccccccccc:$mts_spco_gd_seqno_to_wait_for"
  --eval SET GTID_NEXT = $mts_spco_gd_gtid_to_wait_for_2
  --eval $mts_spco_gd_trx_assigned_worker_2
  SET GTID_NEXT = AUTOMATIC;
}
if ($mts_spco_gd_trx_assigned_worker_3 != '')
{
  --inc $mts_spco_gd_seqno_to_wait_for
  --let $mts_spco_gd_gtid_to_wait_for_3 = "aaaaaaaa-1111-bbbb-2222-cccccccccccc:$mts_spco_gd_seqno_to_wait_for"
  --eval SET GTID_NEXT = $mts_spco_gd_gtid_to_wait_for_3
  --eval $mts_spco_gd_trx_assigned_worker_3
  SET GTID_NEXT = AUTOMATIC;
}

--source include/remove_debug_point.inc

#   2. On the replica, using client connection A, start a transaction and
#      assign it the same GTID as to one of the statements issued on the
#      source.
--source include/rpl_connection_slave.inc
--eval SET GTID_NEXT = $mts_spco_gd_gtid_to_wait_for_1
BEGIN;

if ($mts_spco_gd_trx_assigned_worker_2 != '')
{
  --let $rpl_connection_name = rpl_slave_connection_2
  --source include/rpl_connection.inc
  --eval SET GTID_NEXT = $mts_spco_gd_gtid_to_wait_for_2
  BEGIN;
}
if ($mts_spco_gd_trx_assigned_worker_3 != '')
{
  --let $rpl_connection_name = rpl_slave_connection_3
  --source include/rpl_connection.inc
  --eval SET GTID_NEXT = $mts_spco_gd_gtid_to_wait_for_3
  BEGIN;
}

--source include/rpl_connection_slave1.inc
#   3. Start the replication threads
--source include/start_slave_sql.inc
--let $client = `SELECT CONNECTION_ID()`

--let $assert_text = Replica parallel type is LOGICAL_CLOCK
--let $assert_cond = "[SELECT @@GLOBAL.replica_parallel_type]" = "LOGICAL_CLOCK"
--source include/assert.inc
--let $worker_count = `SELECT COUNT(*) FROM performance_schema.replication_applier_status_by_worker WHERE channel_name='' AND service_state="ON"`
--let $assert_text = MTS worker thread count is correct
--let $assert_cond = "3" = "$worker_count"
--source include/assert.inc
--let $assert_text = Replica preserve commit order is 1
--let $assert_cond = "[SELECT @@GLOBAL.replica_preserve_commit_order]" = "1"
--source include/assert.inc
--let $assert_text = Replica transaction retries has correct configured value
--let $assert_cond = "[SELECT @@GLOBAL.replica_transaction_retries]" = "$mts_spco_gd_transaction_retries"
--source include/assert.inc
--let $assert_text = InnoDB lock wait timeout has correct configured value
--let $assert_cond = "[SELECT @@GLOBAL.innodb_lock_wait_timeout]" = "$mts_spco_gd_innodb_wait_timeout"
--source include/assert.inc

#   4. On the replica, ensure that the applier worker threads are waiting
#      on the pending client connection transaction.
if ($mts_spco_gd_trx_assigned_worker_2 != '')
{
  --let $rpl_connection_name = rpl_slave_connection_2
  --source include/rpl_connection.inc
  ROLLBACK;
  SET GTID_NEXT = AUTOMATIC;
  --echo include/wait_condition.inc [First worker must wait on commit order]
  --let $wait_condition = SELECT count(*) = 1 FROM information_schema.processlist WHERE STATE = "Waiting for preceding transaction to commit"
  --source include/wait_condition.inc
}
if ($mts_spco_gd_trx_assigned_worker_3 != '')
{
  if ($mts_spco_gd_worker_3_only_runs_after_deadlock == 0)
  {
    --source include/wait_condition.inc
    --let $rpl_connection_name = rpl_slave_connection_3
    --source include/rpl_connection.inc
    ROLLBACK;
    SET GTID_NEXT = AUTOMATIC;
    --echo include/wait_condition.inc [Second worker must wait on commit order]
    --let $wait_condition = SELECT count(*) = 2 FROM information_schema.processlist WHERE STATE = "Waiting for preceding transaction to commit"
    --source include/wait_condition.inc
  }
}

#   5. On the replica, using client connection B, execute
#      $mts_spco_gd_trx_blocking_client (to block the first replicated
#      transaction further down the execution), leading to the following
#      lock acquisition dependencies: Client B --statement required lock-->
#      Worker 2 --commit order lock--> Worker 1 --gtid lock--> Client A.
if ($mts_spco_gd_trx_blocking_client != '')
{
  --source include/rpl_connection_slave1.inc
  --send_eval $mts_spco_gd_trx_blocking_client
}

--source include/rpl_connection_slave.inc

#   6. On the replica, ensure that client connection B is waiting on the
#      lock being held by Worker 2 or Worker 3, reaching the state
#      $mts_spco_gd_state_blocking_client.
if ($mts_spco_gd_state_blocking_client != '')
{
  --echo include/wait_condition.inc [Client connection must wait for state]
  --let $_mts_spco_gd_state_blocking_client = $mts_spco_gd_state_blocking_client
  if ($_mts_spco_gd_state_blocking_client == None)
  {
    --let $_mts_spco_gd_state_blocking_client =
  }
  --let $wait_condition = SELECT count(*) = 1 FROM information_schema.processlist WHERE STATE = "$_mts_spco_gd_state_blocking_client" AND ID = $client
  --source include/wait_condition.inc
}

#   7. On the replica, using client connection A, rollback the pending
#      transaction, leading to the following lock acquisition dependencies:
#      Client B --statement required lock--> Worker 2 --commit order
#      lock--> Worker 1 --statement required lock--> Client B.
ROLLBACK;
SET GTID_NEXT = AUTOMATIC;

#   8. Wait for the client connection B to receive its return packet from
#      the server.
if ($mts_spco_gd_trx_blocking_client != '')
{
  --source include/rpl_connection_slave1.inc
  --reap
}

#   9. Wait for the coordinator to reach the
#      $mts_spco_gd_wait_for_coordinator_running_state state before
#      allowing it to error out, if any is defined.
if ($mts_spco_gd_wait_for_coordinator_running_state != '')
{
  --let $slave_param = Slave_SQL_Running_State
  --let $slave_param_value = $mts_spco_gd_wait_for_coordinator_running_state
  --source include/wait_for_slave_param.inc

  if ($mts_spco_gd_trx_to_client_unblocking_workers != '')
  {
    --source include/rpl_connection_slave1.inc
    --eval $mts_spco_gd_trx_to_client_unblocking_workers
  }
}

if ($mts_spco_gd_trx_assigned_worker_3 != '')
{
  if ($mts_spco_gd_worker_3_only_runs_after_deadlock == 1)
  {
    --echo include/wait_condition.inc [Coordinator must wait for workers to stop]
    --let $wait_condition = SELECT count(*) = 1 FROM information_schema.processlist WHERE STATE = "Waiting for workers to exit"
    --source include/wait_condition.inc
    --let $rpl_connection_name = rpl_slave_connection_3
    --source include/rpl_connection.inc
    ROLLBACK;
    SET GTID_NEXT = AUTOMATIC;
  }
}

#  10. Wait for the replica applier thread to error out with
#      $mts_spco_gd_error_expected_replica, if any is defined.
if ($mts_spco_gd_error_expected_replica != 0)
{
  --source include/rpl_connection_slave.inc
  --let $slave_sql_errno = convert_error($mts_spco_gd_error_expected_replica)
  --source include/wait_for_slave_sql_error.inc
}

if ($mts_spco_gd_trx_to_client_unblocking_workers != '')
{
  --source include/rpl_connection_slave1.inc
  --eval $mts_spco_gd_trx_to_client_unblocking_workers
}

if ($mts_spco_gd_trx_finishing_group != '')
{
  --source include/rpl_connection_master.inc
  --eval $mts_spco_gd_trx_finishing_group
}

if ($mts_spco_gd_error_expected_replica != 0)
{
  --source include/rpl_connection_slave.inc
  --source include/start_slave_sql.inc
}

#  11. Synchronize the replica with the source.
--source include/rpl_connection_master.inc
--source include/sync_slave_sql_with_master.inc

--source include/rpl_diff.inc

--source include/rpl_connection_master.inc
--source include/sync_slave_sql_with_master.inc
--source include/stop_slave_sql.inc

--let $mts_spco_gd_trx_blocking_worker_1 =
--let $mts_spco_gd_trx_assigned_worker_2 =
--let $mts_spco_gd_trx_assigned_worker_3 =
--let $mts_spco_gd_worker_3_only_runs_after_deadlock =
--let $mts_spco_gd_trx_blocking_client =
--let $mts_spco_gd_state_blocking_client =
--let $mts_spco_gd_wait_for_coordinator_running_state =
--let $mts_spco_gd_error_expected_replica =
--let $mts_spco_gd_trx_to_client_unblocking_workers =
--let $mts_spco_gd_trx_finishing_group =
