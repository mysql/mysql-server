# ==== Purpose ====
#
# This script tests server behavior when a crash occurs during the
# execution of `XA ROLLBACK`, just before the GTID is added to
# GTID_EXECUTED.
#
# ==== Requirements ====
#
# After server restart:
# R1. The GTID_EXECUTED variable should be updated.
# R2. There shouldn't be any pending XA transactions visible with `XA
#     RECOVER`.
#
# ==== Implementation ====
#
# 1. Setup scenario: create table and insert some records.
# 2. Start and execute an XA transaction containing an insert until before
#    `XA ROLLBACK`.
# 3. Take the `GTID_EXECUTED` state.
# 4. Crash the server during `XA ROLLBACK` execution before the GTID is
#    added to GTID_EXECUTED.
# 5. Restart server and check:
#    a. Error log for messages stating that recovery process didn't find
#       any transaction needing recovery.
#    b. The GTID_EXECUTED variable was updated.
#    c. There aren't any pending XA transaction listed in the output of `XA
#       RECOVER`.
#    d. There aren't changes to the table.
#
# ==== References ====
#
# WL#11300: Crash-safe XA + binary log
#
# Related tests;
#   see common/xa_crash_safe/setup.inc
#
--source include/not_valgrind.inc
--source include/have_debug.inc
--source include/have_debug_sync.inc
--let $option_name = log_bin
--let $option_value = 0
--source include/only_with_option.inc
--let $option_name = gtid_mode
--let $option_value = 'ON'
--source include/only_with_option.inc

# 1. Setup scenario: create table and insert some records.
#
--let $xid_data = xid1
--let $xid = `SELECT CONCAT("X'", LOWER(HEX('$xid_data')), "',X'',1")`
--source common/xa_crash_safe/setup.inc
--let $uuid = 72e9b9d8-8a89-11ec-8f56-6f896ffede71
RESET BINARY LOGS AND GTIDS;

# 2. Start and execute an XA transaction containing an insert until before
#    `XA ROLLBACK`.
#
--connect(con1, localhost, root,,)
--connection con1
--eval SET @@SESSION.gtid_next = '$uuid:1'
--eval XA START $xid
INSERT INTO t1 VALUES (1);
--eval XA END $xid
--eval XA PREPARE $xid

# 3. Take the `GTID_EXECUTED` state.
#
--connection default
--let $before_gtid_executed = `SELECT @@GLOBAL.gtid_executed`

# 4. Crash the server during `XA ROLLBACK` execution before the GTID is
#    added to GTID_EXECUTED.
#
--let $auxiliary_connection = default
--let $statement_connection = con1
--let $statement = SET @@SESSION.gtid_next = '$uuid:2'; XA ROLLBACK $xid
--let $sync_point = before_gtid_externalization
--source include/execute_to_conditional_timestamp_sync_point.inc
--source include/dbug_crash_safe.inc
--source common/xa_crash_safe/cleanup_connection.inc

# 5. Restart server and check:
#
--source include/start_mysqld.inc

# 5.a. Error log for messages stating that recovery process didn't find
#      any transaction needing recovery.
#
--let $assert_select = in InnoDB engine. No attempts to commit, rollback or prepare any transactions.
--source common/xa_crash_safe/assert_recovery_message.inc

# 5.b. The GTID_EXECUTED variable was updated.
#
--let $after_gtid_executed = `SELECT @@GLOBAL.gtid_executed`
--let $assert_text = GTID_EXECUTED has been updated
--let $assert_cond = "$before_gtid_executed" != "$after_gtid_executed"
--source include/assert.inc

# 5.c. There aren't any pending XA transaction listed in the output of `XA
#       RECOVER`.
#
--let $expected_prepared_xa_count = 0
--source common/xa_crash_safe/assert_xa_recover.inc

# 5.d. There aren't changes to the table.
#
--let $expected_row_count = 1
--source common/xa_crash_safe/assert_row_count.inc

DROP TABLE t1;
