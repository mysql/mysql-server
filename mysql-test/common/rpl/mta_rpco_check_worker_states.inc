# This is an auxilary file to use with MTS slave.
# Idea is to make workers in different states and check the output.
# This include file always creates a table in starting and deletes the table in the end
#
# Usage
# [ --let $exec_trx2 = 1 ]
# [ --let $exec_trx4 = 1 ]
# [ --let $exec_trx5 = 1 ]
# --source common/rpl/mta_rpco_check_worker_states.inc
#
# Parameters:
#
# $exec_trx2
# Enable this if worker 2 should be in executing state
# i.e, 'Applying batch of row changes (write)' or 'update' else it will go to 'Waiting for preceding transaction to commit'
#
# $exec_trx4
# Enable this if worker 4 should be in executing state
# i.e, 'Applying batch of row changes (write)' or 'update' else it will go to 'Waiting for preceding transaction to commit'
#
# $exec_trx5
# Enable this if worker 5 should be in executing state
# i.e, 'Applying batch of row changes (write)' or 'update' else it will go to 'Waiting for preceding transaction to commit'
#

# Worker 1 and 3 will be executing state
# i.e, 'Applying batch of row changes (write)' or 'update'
--let $n_executing_workers = 2

--source include/rpl/connection_source.inc

CREATE TABLE t1(a INT PRIMARY KEY);
--source include/rpl/sync_to_replica.inc

--echo # Execute transactions on slave
--source include/rpl/connection_replica.inc

# Save binlog_format
--let $binlog_format = `SELECT @@binlog_format`

# Adding INSERT without commit would block INSERTs from slave worker
# thread and put them in executing state

BEGIN;
  INSERT INTO t1 VALUES (1);
  if ($exec_trx2) {
    INSERT INTO t1 VALUES (2);
    --inc $n_executing_workers
  }
  if ($exec_trx4) {
    INSERT INTO t1 VALUES (4);
    --inc $n_executing_workers
  }
  if ($exec_trx5) {
    INSERT INTO t1 VALUES (5);
    --inc $n_executing_workers
  }

--echo # Block transaction that needs to rollback
--source include/rpl/connection_replica1.inc
BEGIN;
  INSERT INTO t1 VALUES (3);

# Add some data to master with same commit parent.
--source include/rpl/connection_source.inc
--let $debug_point=set_commit_parent_100
--source include/add_debug_point.inc
INSERT INTO t1 VALUES (1);
INSERT INTO t1 VALUES (2);
INSERT INTO t1 VALUES (3);
INSERT INTO t1 VALUES (4);
INSERT INTO t1 VALUES (5);

--source include/rpl/connection_replica.inc

# Check if $n_executing_workers are in blocked state

if ($binlog_format == 'ROW'){
--let $wait_condition= SELECT count(*) = $n_executing_workers FROM performance_schema.threads WHERE PROCESSLIST_STATE="Applying batch of row changes (write)"
--source include/wait_condition_or_abort.inc
}

if ($binlog_format!= 'ROW'){
--let $wait_condition= SELECT count(*) = $n_executing_workers FROM performance_schema.threads WHERE PROCESSLIST_STATE="update"
--source include/wait_condition_or_abort.inc
}

# As $n_executing_workers transaction are blocked the remaining i.e. (5 - $n_executing_workers) should be in waiting state

--let $wait_condition= SELECT count(*) = 5 - $n_executing_workers FROM performance_schema.threads WHERE PROCESSLIST_STATE="Waiting for preceding transaction to commit"
--source include/wait_condition_or_abort.inc

--echo # Commit transaction to make 3rd worker thread rollback
--source include/rpl/connection_replica1.inc
COMMIT;

--echo # Wait until slave worker has failed due to duplicate entry
--let $slave_param= Last_Errno
--let $slave_param_value= convert_error(ER_DUP_ENTRY)
--source include/rpl/wait_for_replica_status.inc

--echo # Release other worker threads
--source include/rpl/connection_replica.inc
ROLLBACK;

--echo # Wait for all worker threads to exit
--let $slave_timeout= 60
--let $slave_param= Replica_SQL_Running
--let $slave_param_value= No
--source include/rpl/wait_for_replica_status.inc

# 3 rows are committed on slave
--let $assert_text= Verify table t1 has 3 values
--let $assert_cond= "[SELECT count(*) COUNT FROM t1, COUNT, 1]" = "3"
--source include/assert.inc

--echo # Delete conflicting transactions on slave
--source include/rpl/connection_replica.inc

BEGIN;
  DELETE FROM t1 WHERE a = 3;
  if ($exec_trx4) {
	  DELETE FROM t1 WHERE a = 4;
  }
  if ($exec_trx5) {
	  DELETE FROM t1 WHERE a = 5;
  }
COMMIT;

--echo # Start replica sql thread
--source include/rpl/start_applier.inc

--let $slave_timeout= 60
--let $slave_param= Replica_SQL_Running_State
--let $slave_param_value= Replica has read all relay log; waiting for more updates
--source include/rpl/wait_for_replica_status.inc

--source include/rpl/connection_source.inc
--source include/rpl/sync_to_replica.inc

--let $assert_text= Verify table t1 has 5 rows
--let $assert_cond= "[SELECT count(*) COUNT FROM t1, COUNT, 1]" = "5"
--source include/assert.inc

--source include/rpl/connection_source.inc

--let $debug_point=set_commit_parent_100
--source include/remove_debug_point.inc

DROP TABLE t1;

--source include/rpl/sync_to_replica.inc
