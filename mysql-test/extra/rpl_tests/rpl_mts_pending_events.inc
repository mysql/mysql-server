###############################################################################
# This .inc is used to check coordinator behaviour of scheduling an event
# when the new event cannot be placed into worker's queue due to size limit
# (REPLICA_PENDING_JOBS_SIZE_MAX)
#
# Usage:
#   --let $table_to_lock=db1.t
#   --let $query_that_waits_on_table_lock=INSERT INTO db1.t VALUES ('small event')
#   --let $query_that_needs_to_be_processed_by_coordinator=INSERT INTO db2.t VALUES (REPEAT('big event', $mts_pending_max))
#   --let $tables_involved_in_test=db1.t, db2.t
#   --source ../mysql-test/extra/rpl_tests/rpl_mts_pending_events.inc
#
# Parameters:
#
#   $table_to_lock:
#     Provide full table name that needs to be locked using LOCK TABLE.
#     Eg: --let $table_to_lock=db1.t
#
#   $query_that_waits_on_table_lock:
#     A query that cannot be executed due the lock acquired by LOCK TABLE.
#     Eg: --let $table_to_lock=db1.t
#         --let $query_that_waits_on_table_lock=INSERT INTO db1.t VALUES ('small event')
#
#   $query_that_needs_to_be_processed_by_coordinator:
#     A query that will be processed by coordinator only after
#     $query_that_waits_on_table_lock is completely executed due to size limit
#     (REPLICA_PENDING_JOBS_SIZE_MAX)
#     --let $query_that_needs_to_be_processed_by_coordinator=INSERT INTO db2.t VALUES (REPEAT('big event', $mts_pending_max))
#
#   $tables_involved_in_test:
#     Tables that are involved in the test. This list will be used to do diff
#     between master and slave's tables to test that replication worked fine.
#     Eg: --let $tables_involved_in_test=db1.t, db2.t
#
###############################################################################

if (!$table_to_lock)
{
  --die ERROR IN TEST: you must set $table_to_lock before sourcing rpl_mts_pending_events.inc.
}
if (!$query_that_waits_on_table_lock)
{
  --die ERROR IN TEST: you must set $query_that_waits_on_table_lock before sourcing rpl_mts_pending_events.inc.
}
if (!$query_that_needs_to_be_processed_by_coordinator)
{
  --die ERROR IN TEST: you must set $query_that_needs_to_be_processed_by_coordinator before sourcing rpl_mts_pending_events.inc.
}
if (!$tables_involved_in_test)
{
  --die ERROR IN TEST: you must set $tables_involved_in_test before sourcing rpl_mts_pending_events.inc.
}

--echo #
--echo #  1) On Slave, lock one table so that any operation on that
--echo #     will be waiting for the lock to be released.
--echo #
--source include/rpl_connection_slave1.inc
--eval LOCK TABLE $table_to_lock WRITE

--echo #
--echo #  2) Execute query that is going to wait for the table lock.
--echo #
--source include/rpl_connection_master.inc
--eval $query_that_waits_on_table_lock

--echo #
--echo #  3) Wait on Slave till a worker picks this event and wait for the
--echo #     lock (which is acquired in step 1)
--echo #
--source include/rpl_connection_slave.inc
--let $wait_condition= SELECT COUNT(*) = 1 FROM information_schema.processlist WHERE state = "Waiting for table metadata lock"
--source include/wait_condition.inc

--echo #
--echo #  4) Now on Master, insert another query that reaches slave.
--echo #
--source include/rpl_connection_master.inc
--eval $query_that_needs_to_be_processed_by_coordinator

--echo #
--echo #  5) Check that Coordinator waits for the query (at step 2) to be
--echo #     executed before dedicating this new query (at step 4) to
--echo #     one of the workers because of the event size limits.
--echo #
--source include/rpl_connection_slave.inc
--let $assert_text= Check that one of the applier worker thread is waiting for the table metadata lock.
--let $assert_cond= [SELECT count(*) FROM information_schema.processlist WHERE state = "Waiting for table metadata lock"] = 1
--source include/assert.inc

--let $wait_condition= SELECT count(*) = 1 FROM information_schema.processlist WHERE state = "Waiting for Replica Workers to free pending events"
--source include/wait_condition.inc

--echo #
--echo #  6) Release the lock acquired in step 1, so that first query will
--echo #     continue it's work and once it is done, second big event
--echo #     will also continue it's work.
--echo #
--source include/rpl_connection_slave1.inc
UNLOCK TABLES;

--echo #
--echo #  7) check that slave is able to catch up with master after releasing the
--echo #     lock in step 6.
--echo #
--echo #     7.1) Sync SQL thread with Master.
--echo #
--source include/rpl_connection_master.inc
--source include/sync_slave_sql_with_master.inc

--echo #
--echo #     7.2) Diff all the tables involved in the test to prove
--echo #          that replication worked fine.
--echo #
--let $number_of_tables_involved_in_test=`SELECT LENGTH('$tables_involved_in_test') - LENGTH(REPLACE('$tables_involved_in_test',',','')) + 1`
--let $t=1
while ($t <= $number_of_tables_involved_in_test)
{
  --let $table= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX('$tables_involved_in_test', ',', $t), ',', -1 )`
  --let $diff_tables= master:$table,slave:$table
  --source include/diff_tables.inc
  --inc $t
}
