# This file is used by rpl_perfschema_connection_applier_status and
# rpl_perfschema_connection_applier_status_mts
#
# This file checks if the performance_schema.replication_applier_status_by_worker
# table displays the correct information after a failed DDL and a failed DML.
#
# First, create a table in the master but do not replicate it to the slave.
# Then, drop the table to force a slave error caused by the failed DDL. Finally,
# check if the last_applied_transaction is the same as before starting this
# transaction and if the applying_transaction is the failed transactions.
# As for the DML, create the same table only in the master. Insert a value in
# the table and wait for the slave to fail. Finally, check again that
# last_applied_transaction did not change and applying_transaction is the failed
# transaction.
#

SET SQL_LOG_BIN= 0;
CREATE TABLE t1(i INT);
SET SQL_LOG_BIN= 1;

--source include/rpl_connection_slave.inc
call mtr.add_suppression("Slave SQL for channel '': .*Error .Unknown table .test.t1.. on query.* Error_code: MY-001051");
call mtr.add_suppression("Slave: Unknown table .test.t1. Error_code: MY-001051");
call mtr.add_suppression("Slave SQL for channel '': .*Error executing row event: 'Table 'test.t1' doesn't exist'");
call mtr.add_suppression("Slave SQL for channel '': ... The slave coordinator and worker threads are stopped, possibly leaving data in inconsistent state.*");
--let $last_applied_trx_before= query_get_value(SELECT LAST_APPLIED_TRANSACTION FROM performance_schema.replication_applier_status_by_worker, LAST_APPLIED_TRANSACTION, 1)

--source include/rpl_connection_master.inc
if ( `SELECT @@GLOBAL.GTID_MODE = 'ON'` )
{
  --let $last_gtid= $current_gtid
  --let $current_gtid= bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb:1
  --disable_query_log
  eval SET SESSION GTID_NEXT= '$current_gtid';
  --enable_query_log
}
DROP TABLE t1;

--source include/rpl_connection_slave.inc
--let $slave_sql_errno= convert_error(ER_BAD_TABLE_ERROR)
-- source include/wait_for_slave_sql_error.inc

--let $last_applied_trx_after= query_get_value(SELECT LAST_APPLIED_TRANSACTION FROM performance_schema.replication_applier_status_by_worker, LAST_APPLIED_TRANSACTION, 1)
--let $applying_trx_after= query_get_value(SELECT APPLYING_TRANSACTION FROM performance_schema.replication_applier_status_by_worker, APPLYING_TRANSACTION, 1)

--let $assert_text= LAST_APPLIED_TRANSACTION did not change after the failed DDL
--let $assert_cond= "$last_applied_trx_after" = "$last_applied_trx_before"
--source include/assert.inc

--let $assert_text= APPLYING_TRANSACTION is equal to the failed transaction GTID
--let $assert_cond= "$applying_trx_after" = "$current_gtid"
--source include/assert.inc

--source include/stop_slave.inc

--source include/rpl_connection_master.inc
if ( `SELECT @@GLOBAL.GTID_MODE = 'ON'` )
{
  --disable_query_log
  SET SESSION GTID_NEXT= AUTOMATIC;
  --enable_query_log
}
--let $rpl_only_running_threads= 1
--source include/rpl_reset.inc

SET SQL_LOG_BIN= 0;
CREATE TABLE t1(i INT);
SET SQL_LOG_BIN= 1;

if ( `SELECT @@GLOBAL.GTID_MODE = 'ON'` )
{
  --let $last_gtid= $current_gtid
  --let $current_gtid= bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb:2
  --disable_query_log
  eval SET SESSION GTID_NEXT= '$current_gtid';
  --enable_query_log
}
INSERT INTO t1 VALUES (1);

--source include/rpl_connection_slave.inc
--let $slave_sql_errno= convert_error(ER_NO_SUCH_TABLE)
-- source include/wait_for_slave_sql_error.inc

--let $last_applied_trx_after= query_get_value(SELECT LAST_APPLIED_TRANSACTION FROM performance_schema.replication_applier_status_by_worker, LAST_APPLIED_TRANSACTION, 1)
--let $applying_trx_after= query_get_value(SELECT APPLYING_TRANSACTION FROM performance_schema.replication_applier_status_by_worker, APPLYING_TRANSACTION, 1)

--let $assert_text= LAST_APPLIED_TRANSACTION did not change after the failed DML
--let $assert_cond= "$last_applied_trx_after" = "$last_applied_trx_before"
--source include/assert.inc

--let $assert_text= APPLYING_TRANSACTION is equal to the failed transaction GTID
--let $assert_cond= "$applying_trx_after" = "$current_gtid"
--source include/assert.inc

# clean up
--source include/rpl_connection_master.inc
if ( `SELECT @@GLOBAL.GTID_MODE = 'ON'` )
{
  --disable_query_log
  SET SESSION GTID_NEXT= AUTOMATIC;
  --enable_query_log
}
SET SQL_LOG_BIN= 0;
DROP TABLE t1;
SET SQL_LOG_BIN= 1;

--let $rpl_only_running_threads= 1
--source include/rpl_reset.inc

--source include/rpl_connection_master.inc
