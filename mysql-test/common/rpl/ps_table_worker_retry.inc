#
# This file is used by rpl.rpl_ps_worker_retry_transaction_mts and
# rpl.rpl_ps_worker_retry_transaction_sts
#
# === Implementation ===
#
# We start by creating a table on the master and then check if all the fields
# related to retrying transactions in the table
# performance_schema.replication_applier_status_by_worker have the expected
# values, more specifically, are empty.
# Next, we start a transaction on the slave to lock the rows of the previously
# created table, so that when the slave replicates a transaction from the master
# updating the same rows in the table, the transaction will block and will have
# to be retried.
# Using a debug synchronization point, we then wait for this transaction to be
# retried twice and six times, and collect the values from the worker's
# performance schema table. Finally, we check that the values are as expected,
# before rolling back the transaction started by the slave, so that the
# transaction replicated from the master is unblocked and can commit.
# We then repeat the process of starting a transaction on the slave to block one
# from the master, but this time the slave will retry the master transaction the
# maximum number of retries, until the applier thread stops. We check that the
# values are as expected before rolling back the transaction started by the
# slave to unblock the master's.
#
# === Parameters ===
#
# $is_mts    1 if MTS is enabled

--source include/have_debug.inc
--source include/have_debug_sync.inc
--source include/not_group_replication_plugin.inc
# test is binlog format agnostic
--source include/have_binlog_format_row.inc

# create channel_1 to server_1 from server_2(slave)
--let $rpl_topology = 1->2
--let $rpl_multi_source = 1
--source include/rpl/init.inc

--let $rpl_connection_name = server_1
--source include/connection.inc
CREATE TABLE t1 (a INT UNIQUE);

--let $rpl_channel_name = 'channel_1'
--let $sync_slave_connection = server_2
--source include/rpl/sync_to_replica.inc

# add suppressions for when MTS is enabled
if ($is_mts) {
  CALL mtr.add_suppression(".*Replica SQL for channel 'channel_1': worker thread retried transaction 10 time.*");
  CALL mtr.add_suppression(".*The replica coordinator and worker threads are stopped, possibly leaving data in inconsistent state.*");
}

# check that the values are empty as expected
--let $ps_table = performance_schema.replication_applier_status_by_worker

--let $last_applied_trx_retries_count = query_get_value(SELECT LAST_APPLIED_TRANSACTION_RETRIES_COUNT FROM $ps_table, LAST_APPLIED_TRANSACTION_RETRIES_COUNT, 1)
--let $last_applied_trx_errno = query_get_value(SELECT LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER FROM $ps_table, LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER, 1)
--let $last_applied_trx_errmsg = query_get_value(SELECT LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE FROM $ps_table, LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE, 1)
--let $last_applied_trx_errts = query_get_value(SELECT LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_TIMESTAMP FROM $ps_table, LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_TIMESTAMP, 1)

--let $applying_trx_retries_count = query_get_value(SELECT APPLYING_TRANSACTION_RETRIES_COUNT FROM $ps_table, APPLYING_TRANSACTION_RETRIES_COUNT, 1)
--let $applying_trx_errno = query_get_value(SELECT APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER FROM $ps_table, APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER, 1)
--let $applying_trx_errmsg = query_get_value(SELECT APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE FROM $ps_table, APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE, 1)
--let $applying_trx_errts = query_get_value(SELECT APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_TIMESTAMP FROM $ps_table, APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_TIMESTAMP, 1)

--let $assert_text = LAST_APPLIED_TRANSACTION_RETRIES_COUNT must be zero for the first transaction
--let $assert_cond = $last_applied_trx_retries_count = 0
--source include/assert.inc

--let $assert_text = LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER must be zero for the first transaction
--let $assert_cond = $last_applied_trx_errno = 0
--source include/assert.inc

--let $assert_text = LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE must be empty for the first transaction
--let $assert_cond = "$last_applied_trx_errmsg" = ""
--source include/assert.inc

--let $assert_text = LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_TIMESTAMP must be zero for the first transaction
--let $assert_cond = "$last_applied_trx_errts" = "0000-00-00 00:00:00.000000"
--source include/assert.inc

--let $assert_text = APPLYING_TRANSACTION_RETRIES_COUNT must be zero when no transaction is being applied
--let $assert_cond = $applying_trx_retries_count = 0
--source include/assert.inc

--let $assert_text = APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER must be zero when no transaction is being applied
--let $assert_cond = $applying_trx_errno = 0
--source include/assert.inc

--let $assert_text = APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE must be empty when no transaction is being applied
--let $assert_cond = "$applying_trx_errmsg" = ""
--source include/assert.inc

--let $assert_text = APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_TIMESTAMP must be zero when no transaction is being applied
--let $assert_cond = "$applying_trx_errts" = "0000-00-00 00:00:00.000000"
--source include/assert.inc

--let $rpl_channel_name =
--source include/rpl/stop_replica.inc

# backup the original value of the variables
SET @saved_innodb_lock_wait_timeout = @@GLOBAL.innodb_lock_wait_timeout;
SET @saved_replica_transaction_retries = @@GLOBAL.replica_transaction_retries;

# set the variables needed for this test
SET GLOBAL replica_transaction_retries = 10;
SET GLOBAL innodb_lock_wait_timeout = 2;

--let $debug_point = rpl_ps_tables_worker_retry
--source include/add_debug_point.inc

# lock the rows of table t1 in the slave
--source include/rpl/start_replica.inc
BEGIN;
INSERT INTO t1 VALUES(1);

# insert the rows on the same table on master so that this transaction will be
# blocked and retried
--let $rpl_connection_name = server_1
--source include/connection.inc
INSERT INTO t1 VALUES(1);

# On slave, check that this transaction from the master has been retried
--let  $rpl_connection_name = server_2
--source include/connection.inc

# collect the values from the table when the transaction has been retried twice
SET debug_sync= 'now WAIT_FOR signal.rpl_ps_tables_worker_retry_pause';
--let $applying_trx_start_apply_two_retries = query_get_value(SELECT APPLYING_TRANSACTION_START_APPLY_TIMESTAMP FROM $ps_table, APPLYING_TRANSACTION_START_APPLY_TIMESTAMP, 1)
--let $applying_trx_retries_count_two_retries = query_get_value(SELECT APPLYING_TRANSACTION_RETRIES_COUNT FROM $ps_table, APPLYING_TRANSACTION_RETRIES_COUNT, 1)
--let $applying_trx_errno_two_retries = query_get_value(SELECT APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER FROM $ps_table, APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER, 1)
--let $applying_trx_errmsg_two_retries = query_get_value(SELECT APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE FROM $ps_table, APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE, 1)
--let $applying_trx_errts_two_retries = query_get_value(SELECT APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_TIMESTAMP FROM $ps_table, APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_TIMESTAMP, 1)
SET debug_sync= 'now SIGNAL signal.rpl_ps_tables_worker_retry_continue';

# collect the values from the table when the same transaction has been retried
# six times
SET debug_sync= 'now WAIT_FOR signal.rpl_ps_tables_worker_retry_pause';
--let $applying_trx_start_apply_six_retries = query_get_value(SELECT APPLYING_TRANSACTION_START_APPLY_TIMESTAMP FROM $ps_table, APPLYING_TRANSACTION_START_APPLY_TIMESTAMP, 1)
--let $applying_trx_retries_count_six_retries = query_get_value(SELECT APPLYING_TRANSACTION_RETRIES_COUNT FROM $ps_table, APPLYING_TRANSACTION_RETRIES_COUNT, 1)
--let $applying_trx_errno_six_retries = query_get_value(SELECT APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER FROM $ps_table, APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER, 1)
--let $applying_trx_errmsg_six_retries = query_get_value(SELECT APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE FROM $ps_table, APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE, 1)
--let $applying_trx_errts_six_retries = query_get_value(SELECT APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_TIMESTAMP FROM $ps_table, APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_TIMESTAMP, 1)
SET debug_sync= 'now SIGNAL signal.rpl_ps_tables_worker_retry_continue';

#check that the collect values are as expected
--let $assert_text = APPLYING_TRANSACTION_RETRIES_COUNT must be 2
--let $assert_cond = $applying_trx_retries_count_two_retries = 2
--source include/assert.inc

--let $lock_wait_timeout_errno = convert_error(ER_LOCK_WAIT_TIMEOUT)
--let $assert_text = APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER must correspond to ER_LOCK_WAIT_TIMEOUT
--let $assert_cond = $applying_trx_errno_two_retries = $lock_wait_timeout_errno
--source include/assert.inc

--let $assert_text = APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE must correspond to ER_LOCK_WAIT_TIMEOUT message
--let $assert_cond = "$applying_trx_errmsg_two_retries" = "Lock wait timeout exceeded; try restarting transaction"
--source include/assert.inc

--let $unix_applying_trx_errts_two_retries = `SELECT UNIX_TIMESTAMP('$applying_trx_errts_two_retries')`
--let $unix_applying_trx_start_apply_two_retries = `SELECT UNIX_TIMESTAMP('$applying_trx_start_apply_two_retries')`
--let $assert_text = APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_TIMESTAMP must be more recent than APPLYING_TRANSACTION_START_APPLY_TIMESTAMP
--let $assert_cond = $unix_applying_trx_errts_two_retries  > $unix_applying_trx_start_apply_two_retries
--source include/assert.inc

--let $unix_applying_trx_start_apply_six_retries = `SELECT UNIX_TIMESTAMP('$applying_trx_start_apply_six_retries')`
--let $assert_text = APPLYING_TRANSACTION_START_APPLY_TIMESTAMP must not have changed between retries
--let $assert_cond = $unix_applying_trx_start_apply_six_retries = $unix_applying_trx_start_apply_two_retries
--source include/assert.inc

--let $assert_text = APPLYING_TRANSACTION_RETRIES_COUNT must increase between retries
--let $assert_cond = $applying_trx_retries_count_six_retries > $applying_trx_retries_count_two_retries
--source include/assert.inc

--let $assert_text = APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER must not have changed between retries
--let $assert_cond = $applying_trx_errno_six_retries = $applying_trx_errno_two_retries
--source include/assert.inc

--let $unix_applying_trx_errts_six_retries = `SELECT UNIX_TIMESTAMP('$applying_trx_errts_six_retries')`
--let $unix_applying_trx_errts_two_retries_previous = `SELECT UNIX_TIMESTAMP('$applying_trx_errts_two_retries')`
--let $assert_text = APPLYING_TRANSACTION_LAST_TRANSIENT_TIMESTAMP must be more recent when the transaction is retried
--let $assert_cond = $unix_applying_trx_errts_six_retries  > $unix_applying_trx_errts_two_retries_previous
--source include/assert.inc

# release the lock so that the master's transaction can commit
ROLLBACK;

--let $rpl_connection_name = server_1
--source include/connection.inc
--let $rpl_channel_name = 'channel_1'
--let $sync_slave_connection = server_2
--source include/rpl/sync_to_replica.inc

# check that the retry fields in applying transaction are empty and that the
# retry fields for the last applied transaction correspond to the ones collected
# in the previous sample while it was applying
--let $last_applied_trx_start_apply = query_get_value(SELECT LAST_APPLIED_TRANSACTION_START_APPLY_TIMESTAMP FROM $ps_table, LAST_APPLIED_TRANSACTION_START_APPLY_TIMESTAMP, 1)
--let $last_applied_trx_end_apply = query_get_value(SELECT LAST_APPLIED_TRANSACTION_END_APPLY_TIMESTAMP FROM $ps_table, LAST_APPLIED_TRANSACTION_END_APPLY_TIMESTAMP, 1)
--let $last_applied_trx_retries_count = query_get_value(SELECT LAST_APPLIED_TRANSACTION_RETRIES_COUNT FROM $ps_table, LAST_APPLIED_TRANSACTION_RETRIES_COUNT, 1)
--let $last_applied_trx_errno = query_get_value(SELECT LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER FROM $ps_table, LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER, 1)
--let $last_applied_trx_errmsg = query_get_value(SELECT LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE FROM $ps_table, LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE, 1)
--let $last_applied_trx_errts = query_get_value(SELECT LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_TIMESTAMP FROM $ps_table, LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_TIMESTAMP, 1)

--let $applying_trx_retries_count = query_get_value(SELECT APPLYING_TRANSACTION_RETRIES_COUNT FROM $ps_table, APPLYING_TRANSACTION_RETRIES_COUNT, 1)
--let $applying_trx_errno = query_get_value(SELECT APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER FROM $ps_table, APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER, 1)
--let $applying_trx_errmsg = query_get_value(SELECT APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE FROM $ps_table, APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE, 1)
--let $applying_trx_errts = query_get_value(SELECT APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_TIMESTAMP FROM $ps_table, APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_TIMESTAMP, 1)

--let $assert_text = LAST_APPLIED_TRANSACTION_RETRIES_COUNT must larger than zero for a retried transaction
--let $assert_cond = $last_applied_trx_retries_count > 0
--source include/assert.inc

--let $assert_text = LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER should be the same as previous APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER
--let $assert_cond = $last_applied_trx_errno = $applying_trx_errno_six_retries
--source include/assert.inc

--let $assert_text = LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE should be the same as previous APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE
--let $assert_cond = "$last_applied_trx_errmsg" = "$applying_trx_errmsg_six_retries"
--source include/assert.inc

--let $unix_last_applied_trx_errts = `SELECT UNIX_TIMESTAMP('$last_applied_trx_errts')`
--let $unix_last_applied_trx_start_apply = `SELECT UNIX_TIMESTAMP('$last_applied_trx_start_apply')`
--let $assert_text = LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_TIMESTAMP must be more recent than LAST_APPLIED_TRANSACTION_START_APPLY_TIMESTAMP
--let $assert_cond = $unix_last_applied_trx_errts  > $unix_last_applied_trx_start_apply
--source include/assert.inc

--let $unix_last_applied_trx_errts = `SELECT UNIX_TIMESTAMP('$last_applied_trx_errts')`
--let $unix_last_applied_trx_end_apply = `SELECT UNIX_TIMESTAMP('$last_applied_trx_end_apply')`
--let $assert_text = LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_TIMESTAMP must be older than LAST_APPLIED_TRANSACTION_END_APPLY_TIMESTAMP
--let $assert_cond = "$unix_last_applied_trx_errts" < "$unix_last_applied_trx_end_apply"
--source include/assert.inc

--let $assert_text = APPLYING_TRANSACTION_RETRIES_COUNT must be zero when no transaction is being applied
--let $assert_cond = $applying_trx_retries_count = 0
--source include/assert.inc

--let $assert_text = APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER must be zero when no transaction is being applied
--let $assert_cond = $applying_trx_errno = 0
--source include/assert.inc

--let $assert_text = APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE must be empty when no transaction is being applied
--let $assert_cond = "$applying_trx_errmsg" = ""
--source include/assert.inc

--let $assert_text = APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_TIMESTAMP must be zero when no transaction is being applied
--let $assert_cond = "$applying_trx_errts" = "0000-00-00 00:00:00.000000"
--source include/assert.inc


# Check that when the slave applier thread stops because it exceeded the
# retries, the information about the last failed retry attempt is preserved.

# remove the debug point so that the slave does not stop during the retries
--let $debug_point = rpl_ps_tables_worker_retry
--source include/remove_debug_point.inc

# lock the rows of table t1 in the slave
BEGIN;
INSERT INTO t1 VALUES(2);

# insert the rows on the same table on master so that this transaction will be
# blocked and retried
--let $rpl_connection_name = server_1
--source include/connection.inc
INSERT INTO t1 VALUES(2);

# On slave, wait until applier thread is stopped
--let $rpl_channel_name =
--let  $rpl_connection_name = server_2
--source include/connection.inc
--let $slave_sql_errno = convert_error(ER_LOCK_WAIT_TIMEOUT)
--source include/rpl/wait_for_applier_to_stop.inc

--let $new_last_applied_trx_start_apply = query_get_value(SELECT LAST_APPLIED_TRANSACTION_START_APPLY_TIMESTAMP FROM $ps_table, LAST_APPLIED_TRANSACTION_START_APPLY_TIMESTAMP, 1)
--let $new_last_applied_trx_end_apply = query_get_value(SELECT LAST_APPLIED_TRANSACTION_END_APPLY_TIMESTAMP FROM $ps_table, LAST_APPLIED_TRANSACTION_END_APPLY_TIMESTAMP, 1)
--let $new_last_applied_trx_retries_count = query_get_value(SELECT LAST_APPLIED_TRANSACTION_RETRIES_COUNT FROM $ps_table, LAST_APPLIED_TRANSACTION_RETRIES_COUNT, 1)
--let $new_last_applied_trx_errno = query_get_value(SELECT LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER FROM $ps_table, LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER, 1)
--let $new_last_applied_trx_errmsg = query_get_value(SELECT LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE FROM $ps_table, LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE, 1)
--let $new_last_applied_trx_errts = query_get_value(SELECT LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_TIMESTAMP FROM $ps_table, LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_TIMESTAMP, 1)

--let $applying_trx_retries_count = query_get_value(SELECT APPLYING_TRANSACTION_RETRIES_COUNT FROM $ps_table, APPLYING_TRANSACTION_RETRIES_COUNT, 1)
--let $applying_trx_errno = query_get_value(SELECT APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER FROM $ps_table, APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER, 1)
--let $applying_trx_errmsg = query_get_value(SELECT APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE FROM $ps_table, APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE, 1)

# unblock the transaction from master
ROLLBACK;
--source include/rpl/start_replica.inc
--let $rpl_connection_name = server_1
--source include/connection.inc
--let $rpl_channel_name = 'channel_1'
--let $sync_slave_connection = server_2
--source include/rpl/sync_to_replica.inc

# check that the fields of applying_transaction and last_applied_transaction
# have the expected information after the slave exhausts the number of retries

--let $assert_text = The information on LAST_APPLIED_TRANSACTION_START_APPLY_TIMESTAMP was kept
--let $assert_cond = "$new_last_applied_trx_start_apply" = "$last_applied_trx_start_apply"
--source include/assert.inc

--let $assert_text = The information on LAST_APPLIED_TRANSACTION_END_APPLY_TIMESTAMP was kept
--let $assert_cond = "$new_last_applied_trx_end_apply" = "$last_applied_trx_end_apply"
--source include/assert.inc

--let $assert_text = The information on LAST_APPLIED_TRANSACTION_RETRIES_COUNT was kept
--let $assert_cond = "$new_last_applied_trx_retries_count" = "$last_applied_trx_retries_count"
--source include/assert.inc

--let $assert_text = The information on LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER was kept
--let $assert_cond = "$new_last_applied_trx_errno" = "$last_applied_trx_errno"
--source include/assert.inc

--let $assert_text = The information on LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE was kept
--let $assert_cond = "$new_last_applied_trx_errmsg" = "$last_applied_trx_errmsg"
--source include/assert.inc

--let $assert_text = The information on LAST_APPLIED_TRANSACTION_LAST_TRANSIENT_ERROR_TIMESTAMP was kept
--let $assert_cond = "$new_last_applied_trx_errts" = "$last_applied_trx_errts"
--source include/assert.inc

--let $slave_trx_retries = `SELECT @@GLOBAL.replica_transaction_retries`
--let $assert_text = APPLYING_TRANSACTION_RETRIES_COUNT is the same as replica_transaction_retries
--let $assert_cond = "$applying_trx_retries_count" = "$slave_trx_retries"
--source include/assert.inc

--let $lock_wait_timeout_errno = convert_error(ER_LOCK_WAIT_TIMEOUT)
--let $assert_text = APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_NUMBER must correspond to ER_LOCK_WAIT_TIMEOUT
--let $assert_cond = $applying_trx_errno = $lock_wait_timeout_errno
--source include/assert.inc

--let $assert_text = APPLYING_TRANSACTION_LAST_TRANSIENT_ERROR_MESSAGE must correspond to ER_LOCK_WAIT_TIMEOUT message
--let $assert_cond = "$applying_trx_errmsg" = "Lock wait timeout exceeded; try restarting transaction"
--source include/assert.inc

# cleanup
--let $rpl_connection_name = server_1
--source include/connection.inc
DROP TABLE t1;
--let $rpl_channel_name = 'channel_1'
--let $sync_slave_connection = server_2
--let $rpl_allow_error= 1
--source include/rpl/sync_to_replica.inc

SET GLOBAL innodb_lock_wait_timeout = @saved_innodb_lock_wait_timeout;
SET GLOBAL replica_transaction_retries = @saved_replica_transaction_retries;

--let $rpl_skip_sync = 1
--source include/rpl/deinit.inc
