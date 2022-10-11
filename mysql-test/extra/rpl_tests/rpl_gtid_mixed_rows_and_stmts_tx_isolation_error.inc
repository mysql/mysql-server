# ==== Purpose ====
#
# This include will generate one transaction on the master mixing RBR and SBR.
#
# The include will then instruct the master to set the flag stating that the
# transaction is pure RBR using debug instrumentation.
#
# Since BUG#25040331 fix there is an optimization on the slave applier that
# will set the transaction isolation level to READ COMMITTED for pure row based
# transactions when the applier's transaction isolation level is more
# restrictive than that.
#
# This optimization is assuming that no QUERY (other than terminal transaction
# ones, like "COMMIT", "ROLLBACK", "XA END") can happen in the middle of a pure
# row based replicated transaction (the flag is set on the GTID event).
#
# The slave applier will error when using the optimization and finding a
# non-terminal QUERY in the middle of a assumed pure row based replicated
# transaction.
#
# ==== Related Bugs and Worklogs ====
#
# BUG#25040331: INTERLEAVED XA TRANSACTIONS MAY DEADLOCK SLAVE APPLIER WITH
#               REPEATABLE READ
#
# ==== Usage ====
#
# --let $use_xa= 0|1
# --source extra/rpl_tests/rpl_gtid_mixed_rows_and_stmts_tx_isolation_error.inc
#
# Parameters:
#
#   $use_xa
#     0: transactions will begin with "BEGIN"
#        and will finish with "COMMIT"
#     1: transactions will begin with "XA START"
#        and will finish with "XA END; XA COMMIT"
#

# The following START slave is taking place just to put a Warning message
# in the slave's error log. This message will be used by the "assert_grep.inc"
# to know where to start counting for lines with the expected pattern.
--source include/rpl_connection_slave.inc
--source include/stop_slave.inc
--source include/start_slave.inc

# Let's generate the workload on the master
--source include/rpl_connection_master.inc
CREATE TABLE t1 (c1 INT);
--source include/sync_slave_sql_with_master.inc

# Make next replicated transactions be assumed as pure RBR
--let $debug_point=force_trx_as_rbr_only
--source include/add_debug_point.inc

--source include/rpl_connection_master.inc
if (!$use_xa)
{
  BEGIN;
}
if ($use_xa)
{
  XA START 'a';
}
INSERT INTO t1 (c1) VALUES (FLOOR((RAND() * 10)));
INSERT INTO t1 (c1) VALUES (11);
INSERT INTO t1 (c1) VALUES (FLOOR((RAND() * 10)));
if (!$use_xa)
{
  COMMIT;
}
if ($use_xa)
{
  XA END 'a';
  XA PREPARE 'a';
  XA COMMIT 'a';
}
--source include/sync_slave_io_with_master.inc

# Wait until the slave reports the error
--let $slave_sql_errno= convert_error(ER_REPLICA_FATAL_ERROR)
--source include/wait_for_slave_sql_error.inc

--source include/remove_debug_point.inc

# Assert that the error message was logged on slave
--let $assert_file=$MYSQLTEST_VARDIR/tmp/slave.err
# Show entries only after the last occurrence of the following pattern
# produced by the STOP/START slave in the beginning of this include file
--let $assert_only_after=.* \[Warning\] Storing MySQL user name or password information
# Each test will log only one error line
# (one per QUERY mixed with rows events)
--let $assert_count= 1
# Assert that the expected warning line is in the log
--let $assert_select=.* \[ERROR\] .* an unexpected statement was found in the middle of the transaction
--let $assert_text= Found the expected warning line in the error log.
--source include/assert_grep.inc

# Cleanup
--source include/start_slave.inc
--source include/rpl_connection_master.inc
DROP TABLE t1;
--source include/sync_slave_sql_with_master.inc
