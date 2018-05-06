# This file is used by rpl.rpl_perfschema_connection_applier_status and
# rpl.rpl_perfschema_connection_applier_status_mts
#
# Five transaction types are tested:
#  - create table
#  - insert
#  - update
#  - delete
#  - drop table
# For each statement, the test asserts that the information concerning the
# transaction that is currently being processed matches what is expected
# before and after each of the following replication stages is completed for
# that transaction:
#  1) The connection thread queues the transaction to be applied by the applier
#     thread (table: performance_schema.replication_connection_status)
#  2) If MTS is enabled, the coordinator puts the transaction into a worker's
#     queue (table: performance_schema.replication_applier_status_by_coordinator)
#  3) The applier thread applies the transaction
#     (table: performance_schema.replication_applier_status_by_worker)
#
# param: $is_mts        [0,1] 1 if MTS test


--let $current_gtid= ANONYMOUS

--source include/rpl_connection_slave.inc
--let $last_gtid= query_get_value(select LAST_QUEUED_TRANSACTION from performance_schema.replication_connection_status, LAST_QUEUED_TRANSACTION, 1)
# when an mts slave is stopped, the table
# performance_schema.replication_applier_status_by_worker is cleared, which
# means that LAST_APPLIED_TRANSACTION will be void.
--let $last_gtid_applied= query_get_value(select LAST_APPLIED_TRANSACTION from performance_schema.replication_applier_status_by_worker, LAST_APPLIED_TRANSACTION, 1)
--let $debug_point= rpl_ps_tables
--source include/add_debug_point.inc

--source include/stop_slave_io.inc
--source include/rpl_connection_master.inc

if ( `SELECT @@GLOBAL.GTID_MODE = 'ON'` )
{
  --let $current_gtid= aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1
  --disable_query_log
  eval SET SESSION GTID_NEXT= '$current_gtid';
  --enable_query_log
}
CREATE TABLE t (a int);

--let $is_first_stmt= 1
--source extra/rpl_tests/rpl_check_ps_connection_applier_status_tables.inc
--let $is_first_stmt= 0

--source include/stop_slave_io.inc
--source include/rpl_connection_master.inc
--let $last_gtid= $current_gtid
if ( `SELECT @@GLOBAL.GTID_MODE = 'ON'` )
{
  --let $current_gtid= aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2
  --disable_query_log
  eval SET SESSION GTID_NEXT= '$current_gtid';
  --enable_query_log
}
INSERT INTO t VALUES(1);
--source extra/rpl_tests/rpl_check_ps_connection_applier_status_tables.inc

--source include/stop_slave_io.inc
--source include/rpl_connection_master.inc
if ( `SELECT @@GLOBAL.GTID_MODE = 'ON'` )
{
  --let $last_gtid= $current_gtid
  --let $current_gtid= aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:3
  --disable_query_log
  eval SET SESSION GTID_NEXT= '$current_gtid';
  --enable_query_log
}
UPDATE t SET a=2;
--source extra/rpl_tests/rpl_check_ps_connection_applier_status_tables.inc

--source include/stop_slave_io.inc
--source include/rpl_connection_master.inc
if ( `SELECT @@GLOBAL.GTID_MODE = 'ON'` )
{
  --let $last_gtid= $current_gtid
  --let $current_gtid= aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:4
  --disable_query_log
  eval SET SESSION GTID_NEXT= '$current_gtid';
  --enable_query_log
}
DELETE FROM t WHERE a=2;
--source extra/rpl_tests/rpl_check_ps_connection_applier_status_tables.inc

--source include/stop_slave_io.inc
--source include/rpl_connection_master.inc
if ( `SELECT @@GLOBAL.GTID_MODE = 'ON'` )
{
  --let $last_gtid= $current_gtid
  --let $current_gtid= aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:5
  --disable_query_log
  eval SET SESSION GTID_NEXT= '$current_gtid';
  --enable_query_log
}
DROP TABLE t;
--source extra/rpl_tests/rpl_check_ps_connection_applier_status_tables.inc

--let $debug_point= rpl_ps_tables
--source include/remove_debug_point.inc

# Assert that the information in the worker table is kept when the slave is
# stopped and started in case of a STS, or that assert that the
# information in the worker table is kept and cleared correctly when the slave
# is stopped and restarted when MTS is in use.
--let $last_applied_gtid_before_stop= query_get_value(select LAST_APPLIED_TRANSACTION from performance_schema.replication_applier_status_by_worker, LAST_APPLIED_TRANSACTION, 1)
--source include/stop_slave.inc
--let $last_applied_gtid= query_get_value(select LAST_APPLIED_TRANSACTION from performance_schema.replication_applier_status_by_worker, LAST_APPLIED_TRANSACTION, 1)
--let $assert_text= Assert that the gtid is kept while the slave is stopped
--let $assert_cond= "$last_applied_gtid" = "$last_applied_gtid_before_stop"
--source include/assert.inc
--source include/start_slave.inc
--let $last_applied_gtid= query_get_value(select LAST_APPLIED_TRANSACTION from performance_schema.replication_applier_status_by_worker, LAST_APPLIED_TRANSACTION, 1)
if ( $is_mts )
{
  --let $assert_text= Assert that the gtid is clear because the slave was stopped and started
  --let $assert_cond= "$last_applied_gtid" = ""
}
if ( !$is_mts)
{
  --let $assert_text= Assert that the gtid is kept after the slave was stopped and started
  --let $assert_cond= "$last_applied_gtid" = "$last_applied_gtid_before_stop"
}
--source include/assert.inc


--source include/rpl_connection_master.inc
SET SESSION GTID_NEXT= AUTOMATIC;
--source include/sync_slave_sql_with_master.inc

--source include/rpl_connection_master.inc
