# ==== Purpose ====
#
# This script tests server behavior when a crash occurs during the
# execution of `XA PREPARE` and just before the transaction is prepared in
# the internal transaction coordinator, when binary logging is disabled.
#
# ==== Requirements ====
#
# Pre-conditions:
# PC1. Binary logging is disabled.
#
# After server restart:
# R1. There shouldn't be any changes to the data.
# R2. There shouldn't be any pending XA transactions visible with `XA
#     RECOVER`.
#
# ==== Implementation ====
#
# 1. Setup scenario: create table and insert some records.
# 2. Start and execute an XA transaction containing an insert until before
#    `XA PREPARE`.
# 3. Crash the server during `XA PREPARE` execution, after preparing in the
#    storage engines and just before preparing in the internal transaction
#    coordinator.
# 4. Restart server and check:
#    a. Error log for messages stating that recovery process found one
#       transaction needing recovery.
#    b. There aren't any pending XA transaction listed in the output of `XA
#       RECOVER`.
#    c. There aren't changes to the table.
#
# ==== References ====
#
# WL#11300: Crash-safe XA + binary log
#
# Related tests:
#   see common/xa_crash_safe/setup.inc
#
--source include/not_valgrind.inc
--source include/have_debug.inc
--source include/have_debug_sync.inc
--let $option_name = log_bin
--let $option_value = 0
--source include/only_with_option.inc

# 1. Setup scenario: create table and insert some records.
#
--let $xid_data = xid1
--let $xid = `SELECT CONCAT("X'", LOWER(HEX('$xid_data')), "',X'',1")`
--source common/xa_crash_safe/setup.inc

# 2. Start and execute an XA transaction containing an insert until before
#    `XA PREPARE`.
#
--connect(con1, localhost, root,,)
--connection con1
--eval XA START $xid
INSERT INTO t1 VALUES (1);
--eval XA END $xid

# 3. Crash the server during `XA PREPARE` execution, after preparing in the
#    storage engines and just before preparing in the internal transaction
#    coordinator.
#
--let $auxiliary_connection = default
--let $statement_connection = con1
--let $statement = XA PREPARE $xid
--let $sync_point = before_set_prepared_in_tc
--source include/execute_to_conditional_timestamp_sync_point.inc
--source include/kill_mysqld.inc
--source common/xa_crash_safe/cleanup_connection.inc

# 4. Restart server and check:
#
--source include/start_mysqld.inc

# 4.a. Error log for messages stating that recovery process found one
#      transaction needing recovery.
#
--let $assert_select = ((Successfully rolled back 1 XA transaction)|(Rolling back trx with id ([0-9]+), 1 rows to undo))
--source common/xa_crash_safe/assert_recovery_message.inc

# 4.b. There aren't any pending XA transaction listed in the output of `XA
#      RECOVER`.
#
--let $expected_prepared_xa_count = 0
--source common/xa_crash_safe/assert_xa_recover.inc

# 4.c. There aren't changes to the table.
#
--let $expected_row_count = 1
--source common/xa_crash_safe/assert_row_count.inc

DROP TABLE t1;
