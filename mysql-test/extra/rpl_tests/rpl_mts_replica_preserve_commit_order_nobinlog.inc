# ==== Purpose ====
#
# This include is for all common functionality of mts
# replica_preserve_commit_order testcases.
#
# ==== Usage ====
#
#   [--let $mts_spco_skip_init_statement= 0]
#   [--let $mts_replica_parallel_workers= 0]
#   [--let $mts_spco_start_statement= CREATE TABLE t1 (c1 INT);]
#   [--let $mts_spco_start_statement_delim= CREATE PROCEDURE proc_t1 (OUT param1 INT) BEGIN SELECT 'test'; END]
#   [--let $mts_spco_block_statement= BEGIN; INSERT INTO t1 VALUES (1);]
#   [--let $mts_add_debug_point= debug_sync_point_name
#   [--let $mts_spco_parallel_statement1= BEGIN; INSERT INTO t1 VALUES (1); INSERT INTO t2 VALUES (2); COMMIT;]
#   [--let $mts_spco_parallel_statement2= BEGIN; INSERT INTO t2 VALUES (3); INSERT INTO t2 VALUES (4); COMMIT;]
#   [--let $mts_spco_assert_textN= Verify table t1 is empty]
#   [--let $mts_spco_assert_condN= "[SELECT count(*) COUNT FROM t1, COUNT, 1]" = "0"]
#   [--let $mts_spco_wait_condN= "[SELECT count(*) COUNT FROM t1, COUNT, 1]" = "0"]
#   [--let $mts_spco_rollback_statement= 1]
#   [--let $mts_spco_save_gtid_executed= 1]
#   [--let $mts_spco_check_gtid_executed_before_rollback= 1]
#   [--let $mts_spco_end_statement= 1]
#   [--let $mts_spco_cleanup_statement= 1]
#   --source include/rpl_mts_replica_preserve_commit_order_nobinlog.inc
#
# Parameters:
#   $mts_spco_skip_init_statement
#     If set to 1 the initialization statements will be skipped.
#     If set to 0 the initialization statements won't be skipped.
#
#   $mts_spco_start_statement
#     If set will execute $mts_spco_start_statement on master server.
#
#   $mts_spco_start_statement_delim
#     If set it sets 'DELIMITER \\' before executing
#     $mts_spco_start_statement_delim statement and resets delimeter back to ;
#     ('DELIMITER ;') after execution. Its useful to test create procedure
#     statement.
#
#   $mts_replica_parallel_workers
#     If set other then 0 its value its used to set replica_parallel_workers
#     system variable, otherwise its set to value of 2.
#
#   $mts_num_preceding_trans
#     The number of preceding transaction waiting to commit.
#     The default value if not set is 1.
#
#   $mts_spco_block_statement
#     If set will execute $mts_spco_block_statement on slave server.
#     It is used to block $mts_spco_parallel_statement1 or
#     $mts_spco_parallel_statement2 applied through slave applier threads.
#
#   $mts_add_debug_point
#     If set will add a debug point to the current GLOBAL debug
#     variable without changing other debugging flags set and
#     remove the added debug point from GLOBAL before cleanup.
#
#   $mts_spco_parallel_statement1
#     If set will execute $mts_spco_parallel_statement1 on master server.
#     Before executing it, debug macro set_commit_parent_100 is set, which
#     will make last_committed same for all the $mts_spco_parallel_statement1
#     statements, so that they can run in parallel on slave.
#
#   $mts_spco_parallel_statement2
#     If set will execute $mts_spco_parallel_statement2 on master server.
#     Before executing it, debug macro set_commit_parent_100 is set, which
#     will make last_committed same for all the $mts_spco_parallel_statement2
#     statements, so that they can run in parallel on slave.
#
#   $mts_spco_assert_textN
#     where N is integer from 1..N, and should start with 1.
#     It will be executed on slave server.
#     $mts_spco_assert_textN sets $assert_text for include/assert.inc
#     See include/assert.inc for more details.
#
#   $mts_spco_assert_condN
#     where N is integer from 1..N, and should start with 1.
#     It will be executed on slave server.
#     $mts_spco_assert_condN sets $assert_cond for include/assert.inc
#     See include/assert.inc for more details.
#
#   $mts_spco_wait_condN
#     where N is integer from 1..N, and should start with 1.
#     It will be executed on slave server.
#     $mts_spco_wait_condN sets $wait_cond for include/wait_condition.inc
#     See include/wait_condition.inc for more details.
#
#   $mts_spco_rollback_statement
#     If set will execute $mts_spco_rollback_statement on slave server.
#     It is used to rollback earlier blocked $mts_spco_block_statement.
#
#   $mts_spco_save_gtid_executed
#     If set will save @@GLOBAL.GTID_EXECUTED value in variable
#     $gtid_executed_internal_val1 and $gtid_executed_internal_val2 having
#     gtid values before and after $mts_spco_parallel_statementN execution.
#     It will be executed on slave server.
#     Check for its position which may be needed for test.
#
#   $mts_spco_check_gtid_executed_before_rollback
#     If set will assert the difference between $gtid_executed_internal_val2
#     and $gtid_executed_internal_val1 is equal to
#     $mts_spco_check_gtid_executed_before_rollback value. It will be executed
#     on slave server.
#
#   $mts_spco_end_statement
#     If set execute $mts_spco_end_statement statement on master server.
#     It is executed at end of the testcase to drop or cleanup tables.
#
#   $mts_spco_cleanup_statement
#     If set execute $mts_spco_cleanup_statement statement on master server.
#     It resets variables set during $mts_spco_skip_init_statement.

--echo
--echo # Setup
--echo


if (!$mts_replica_parallel_workers)
{
  --let $mts_replica_parallel_workers = 2
}


if (!$mts_num_preceding_trans)
{
  --let $mts_num_preceding_trans = 1
}


if (!$mts_spco_skip_init_statement)
{
  --source include/rpl_connection_slave.inc

  CALL mtr.add_suppression("You need to use --log-bin to make --binlog-format work");

  SET @save_replica_parallel_workers= @@global.replica_parallel_workers;
  SET @save_replica_parallel_type= @@global.replica_parallel_type;
  SET @save_replica_preserve_commit_order= @@global.replica_preserve_commit_order;
  SET GLOBAL replica_parallel_type = 'LOGICAL_CLOCK';
  --eval SET GLOBAL replica_parallel_workers= $mts_replica_parallel_workers
  SET GLOBAL replica_preserve_commit_order= ON;
  --source include/start_slave.inc

  --source include/rpl_connection_master.inc

  SET @save_binlog_transaction_dependency_tracking= @@global.binlog_transaction_dependency_tracking;
  SET @@global.binlog_transaction_dependency_tracking = COMMIT_ORDER;

  --let $mts_spco_skip_init_statement = 1
}

--let $_tmp_mts_spco_start_statement_delim = 1
if ($mts_spco_start_statement_delim == '')
{
  --let $_tmp_mts_spco_start_statement_delim = 0
}

if ($mts_spco_start_statement)
{
  --source include/rpl_connection_master.inc
  if ($gtid_mode_on)
  {
    --let $use_gtids= 1
    --let $ignore_gtids_on_sync=
    --let $wait_for_executed_gtid_set= 1
  }

  --eval $mts_spco_start_statement

  if ($_tmp_mts_spco_start_statement_delim == 1)
  {
    --DELIMITER //
    --eval $mts_spco_start_statement_delim
    --DELIMITER ;
  }

  --source include/sync_slave_sql_with_master.inc

  if ($gtid_mode_on)
  {
   --let $use_gtids=
   --let $ignore_gtids_on_sync= 1
   --let $wait_for_executed_gtid_set=
  }
}

if ($mts_spco_block_statement)
{
  --echo
  --echo # Block slave sql applier threads
  --echo

  --source include/rpl_connection_slave.inc
  --eval $mts_spco_block_statement
}

if ($mts_add_debug_point)
{
  --source include/rpl_connection_slave.inc
  --let $debug_point=$mts_add_debug_point
  --source include/add_debug_point.inc
}

if ($mts_spco_save_gtid_executed)
{
  --source include/rpl_connection_slave.inc
  --let $gtid_executed_internal_val1=
  if ($gtid_mode_on)
  {
    --source include/rpl_connection_slave.inc
    --let $gtid_executed_internal_val1= `SELECT @@GLOBAL.GTID_EXECUTED`
  }
}

--let $mts_spco_parallel_statement = 0
if ($mts_spco_parallel_statement1)
{
  --let $mts_spco_parallel_statement = 1
}

if ($mts_spco_parallel_statement2)
{
  --let $mts_spco_parallel_statement = 1
}


if ($mts_spco_parallel_statement)
{
  --echo
  --echo # Generate the transactions which can be applied in parallel on slave
  --echo

  --source include/rpl_connection_master.inc

  # Make the following INSERTs have same commit parent. So they can be applied
  # parallel on slave.
  --let $debug_point=set_commit_parent_100
  --source include/add_debug_point.inc
}

if ($mts_spco_parallel_statement1)
{
  --eval $mts_spco_parallel_statement1
}

if ($mts_spco_parallel_statement2)
{
  --let $rpl_connection_name= server_1
  --source include/rpl_connection.inc
  --eval $mts_spco_parallel_statement2
}

if ($mts_spco_parallel_statement)
{
  --connection server_1
  --let $debug_point=set_commit_parent_100
  --source include/remove_debug_point.inc
}

if ($mts_spco_assert_text1)
{
  --echo
  --echo # Verify the transactions are ordered correctly on slave
  --echo

  --let $rpl_connection_name= server_2
  --source include/rpl_connection.inc

  #
  # First insert will block and thus the second
  # one will wait with the commit until the first
  # insert is unblocked
  #
  --let $wait_condition= SELECT count(*) = $mts_num_preceding_trans FROM performance_schema.threads WHERE PROCESSLIST_STATE="Waiting for preceding transaction to commit"
  --source include/wait_condition_or_abort.inc


  # Show that there is no data in the tables.
  --let $i = 1
  while ($i >= 1)
  {
    --let $assert_text = \$mts_spco_assert_text$i
    --let $assert_cond = \$mts_spco_assert_cond$i
    --source include/assert.inc

    --let \$mts_spco_assert_text\$i=
    --let \$mts_spco_assert_cond\$i=
    --inc $i

    --let $_assert_text = \$mts_spco_assert_text$i
    if (!$_assert_text)
    {
      --let $i = 0
    }
  }
}

if ($mts_spco_wait_cond1)
{
  --echo
  --echo # Wait for condition on slave
  --echo

  --let $rpl_connection_name= server_2
  --source include/rpl_connection.inc

  --let $i = 1
  while ($i >= 1)
  {
    --let $wait_condition = \$mts_spco_wait_cond$i
    --source include/wait_condition.inc

    --let \$mts_spco_wait_cond\$i=
    --inc $i

    --let $_wait_cond = \$mts_spco_wait_cond$i
    if (!$_wait_cond)
    {
      --let $i = 0
    }
  }
}

if ($mts_spco_save_gtid_executed)
{
  --let $gtid_executed_internal_val2=
  if ($gtid_mode_on)
  {
    --source include/rpl_connection_slave.inc
    --let $gtid_executed_internal_val2= `SELECT @@GLOBAL.GTID_EXECUTED`
    --let $_tmp_log_bin= `SELECT @@global.log_bin`
    --let $_tmp_log_replica_updates= `SELECT @@global.log_replica_updates`

    if ($_tmp_log_bin == ON)
    {
      if ($_tmp_log_replica_updates == ON)
      {
        if ($mts_spco_check_gtid_executed_before_rollback_exception)
        {
          --let $mts_spco_check_gtid_executed_before_rollback = $mts_spco_check_gtid_executed_before_rollback_exception
        }
      }
    }

    --let $assert_text= Exactly $mts_spco_check_gtid_executed_before_rollback GTIDs should have been committed since last invocation
    --let $assert_cond= GTID_COUNT(GTID_SUBTRACT("$gtid_executed_internal_val2", "$gtid_executed_internal_val1")) = $mts_spco_check_gtid_executed_before_rollback
    --source include/assert.inc
    --let $mts_spco_check_gtid_executed_before_rollback_exception =
  }
}


if ($mts_spco_rollback_statement)
{
  --echo
  --echo # Rollback the first insert so that slave applier threads can
  --echo # unblock and proceed. Verify the transactions are applied.
  --echo

  --source include/rpl_connection_slave.inc
  --eval $mts_spco_rollback_statement
}

if ($mts_add_debug_point)
{
  --source include/rpl_connection_slave.inc
  --let $debug_point=$mts_add_debug_point
  --source include/remove_debug_point.inc
  --let $mts_add_debug_point =
}

if ($mts_spco_end_statement)
{
  --connection server_1
  --eval $mts_spco_end_statement
  --source include/sync_slave_sql_with_master.inc
}

if ($mts_spco_cleanup_statement)
{
  --echo
  --echo # Cleanup
  --echo

  --source include/rpl_connection_master.inc
  SET GLOBAL binlog_transaction_dependency_tracking=@save_binlog_transaction_dependency_tracking;
  --source include/sync_slave_sql_with_master.inc

  --source include/stop_slave.inc

  SET GLOBAL replica_parallel_type=@save_replica_parallel_type;
  --disable_warnings
  SET GLOBAL replica_parallel_workers=@save_replica_parallel_workers;
  --enable_warnings
  SET GLOBAL replica_preserve_commit_order=@save_replica_preserve_commit_order;
  --source include/start_slave.inc

  --source include/rpl_end.inc

  --let $mts_spco_end_statement =
}

--let $mts_spco_start_statement_delim =
--let $mts_ignore_wait_for_preceding_transaction = 0

--let $mts_spco_assert_text1 =
--let $mts_spco_assert_cond1 =

--let $mts_spco_assert_text2 =
--let $mts_spco_assert_cond2 =

--let $mts_spco_assert_text3 =
--let $mts_spco_assert_cond3 =
