# This file is used by rpl.rpl_perfschema_applier_status_by_worker_slave_skip_counter
# and rpl.rpl_perfschema_applier_status_by_worker_slave_skip_counter_mts
#
# Stop the slave to set the slave_skip_counter to 4 events (two transactions).
# Run the transactions on the master and sync the slave with the master.
# Check that the last_queued_transaction shows the skipped transactions.
# If mts is enabled, check that the slave's ps coordinator table also doesn't
# show any queued transactions.
# Check that the slave's ps worker table does not show any transaction applied.
# Also check that the *ing_transaction was cleared in the two (mts, three)
# tables.
#
# param: $is_mts        [0,1] 1 if MTS test


--source include/rpl_connection_slave.inc
--source include/stop_slave_sql.inc
SET GLOBAL sql_replica_skip_counter= 4;

--source include/rpl_connection_master.inc
CREATE TABLE t1 (a int);
DROP TABLE t1;

--source include/rpl_connection_slave.inc
--source include/start_slave_sql.inc
--source include/rpl_connection_master.inc
--source include/sync_slave_sql_with_master.inc

--let $queueing_gtid_= query_get_value(SELECT QUEUEING_TRANSACTION FROM performance_schema.replication_connection_status, QUEUEING_TRANSACTION, 1)
--let $assert_text= queueing_gtid was cleared after the gtid skipped transactions
--let $assert_cond= "$queueing_gtid" = ""
--source include/assert.inc

if ($is_mts)
{
  --let $processing_gtid_= query_get_value(SELECT PROCESSING_TRANSACTION FROM performance_schema.replication_applier_status_by_coordinator, PROCESSING_TRANSACTION, 1)
  --let $assert_text= processing_gtid was cleared after the gtid skipped transactions
  --let $assert_cond= "$processing_gtid" = ""
  --source include/assert.inc
}

--let $applying_gtid= query_get_value(SELECT APPLYING_TRANSACTION FROM performance_schema.replication_applier_status_by_worker, APPLYING_TRANSACTION, 1)
--let $assert_text= applying_gtid was cleared after the gtid skipped transactions
--let $assert_cond= "$applying_gtid" = ""
--source include/assert.inc

--let $last_gtid= query_get_value(SELECT LAST_QUEUED_TRANSACTION FROM performance_schema.replication_connection_status, LAST_QUEUED_TRANSACTION, 1)
--let $assert_text= Assert that the queued gtid is not clear even after the slave skipped the two transactions
--let $assert_cond= "$last_gtid" != ""
--source include/assert.inc

if ( $is_mts )
{
  --let $last_gtid= query_get_value(SELECT LAST_PROCESSED_TRANSACTION FROM performance_schema.replication_applier_status_by_coordinator, LAST_PROCESSED_TRANSACTION, 1)
  --let $assert_text= Assert that the coordinator last_processed gtid is clear after the slave skipped the two transactions
  --let $assert_cond= "$last_gtid" = ""
  --source include/assert.inc
}

--let $last_gtid= query_get_value(SELECT LAST_APPLIED_TRANSACTION FROM performance_schema.replication_applier_status_by_worker, LAST_APPLIED_TRANSACTION, 1)
--let $assert_text= Assert that the applied gtid is clear after the slave skipped the two transactions
--let $assert_cond= "$last_gtid" = ""
--source include/assert.inc
