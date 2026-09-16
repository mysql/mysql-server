# Common test body for XA COMMIT/XA ROLLBACK crash recovery after the
# before-GTID state-update actions ran and before GTID state update itself.
#
# Parameters:
# $replica_has_binlog
#   1 when the replica runs with binary logging enabled, 0 otherwise.

if ($replica_has_binlog == '') {
  --die ERROR IN TEST: replica_has_binlog must be set
}

--source include/have_binlog_format_row.inc
--source include/not_valgrind.inc
--source include/have_debug.inc
--source include/have_debug_sync.inc
--source include/rpl/init_source_replica.inc

if ($replica_has_binlog == 0) {
  --source include/rpl/connection_replica.inc
  CALL mtr.add_suppression("You need to use --log-bin to make --binlog-format work");
}

--source include/rpl/connection_source.inc
--let $xid_data = xid_setup
--let $xid = `SELECT CONCAT("X'", LOWER(HEX('$xid_data')), "',X'',1")`
--source common/xa_crash_safe/setup.inc
--source include/rpl/sync_to_replica.inc

--source include/rpl/connection_source1.inc
--let $xid_data = xid_commit
--let $xid = `SELECT CONCAT("X'", LOWER(HEX('$xid_data')), "',X'',1")`
--eval XA START $xid
INSERT INTO t1 VALUES (1);
--eval XA END $xid
--eval XA PREPARE $xid

--source include/rpl/connection_source.inc
--source include/rpl/sync_to_replica.inc
--let $expected_prepared_xa_count = 1
--source common/xa_crash_safe/assert_xa_recover.inc
--let $before_gtid_executed = `SELECT @@GLOBAL.gtid_executed`
--source include/rpl/connection_source.inc
--let $source_gtid_executed_before_second_phase = `SELECT @@GLOBAL.gtid_executed`

--let $auxiliary_connection = slave
--let $statement_connection = master1
--let $statement = XA COMMIT $xid
--let $sync_point = after_call_actions_before_gtid_state_update
--source include/execute_to_conditional_timestamp_sync_point.inc

--source include/rpl/connection_replica.inc
--let $expected_prepared_xa_count = 0
--source common/xa_crash_safe/assert_xa_recover.inc
--error ER_XAER_DUPID
--eval XA START $xid

--source include/rpl/connection_source.inc
--let $source_gtid_executed_after_second_phase = `SELECT @@GLOBAL.gtid_executed`
--let $second_phase_gtid = `SELECT GTID_SUBTRACT('$source_gtid_executed_after_second_phase', '$source_gtid_executed_before_second_phase')`
--let $assert_text = XA second-phase GTID must be non-empty
--let $assert_cond = "$second_phase_gtid" != ""
--source include/assert.inc

--source include/rpl/connection_replica.inc
--source include/dbug_crash_safe.inc

if ($replica_has_binlog == 0) {
  --let $restart_parameters=restart:--skip-replica-start --skip-log-bin --log-replica-updates=0
}
if ($replica_has_binlog == 1) {
  --let $restart_parameters=restart:--skip-replica-start
}
--source include/start_mysqld.inc
--let $rpl_server_number= 2
--source include/rpl/reconnect.inc

--source include/rpl/connection_replica.inc
--let $after_gtid_executed = `SELECT @@GLOBAL.gtid_executed`
--let $assert_text = GTID_EXECUTED has been updated after XA COMMIT recovery
--let $assert_cond = "$before_gtid_executed" != "$after_gtid_executed"
--source include/assert.inc
--let $assert_text = GTID_EXECUTED contains the recovered XA COMMIT GTID
--let $has_second_phase_gtid = `SELECT GTID_SUBSET('$second_phase_gtid', @@GLOBAL.gtid_executed)`
--let $assert_cond = "$has_second_phase_gtid" = "1"
--source include/assert.inc

--let $expected_prepared_xa_count = 0
--source common/xa_crash_safe/assert_xa_recover.inc

--let $expected_row_count = 2
--source common/xa_crash_safe/assert_row_count.inc

--source include/rpl/start_replica.inc

--source include/rpl/connection_source1.inc
--let $xid_data = xid_rollback
--let $xid = `SELECT CONCAT("X'", LOWER(HEX('$xid_data')), "',X'',1")`
--eval XA START $xid
INSERT INTO t1 VALUES (3);
--eval XA END $xid
--eval XA PREPARE $xid

--source include/rpl/connection_source.inc
--source include/rpl/sync_to_replica.inc
--let $expected_prepared_xa_count = 1
--source common/xa_crash_safe/assert_xa_recover.inc
--let $before_gtid_executed = `SELECT @@GLOBAL.gtid_executed`
--source include/rpl/connection_source.inc
--let $source_gtid_executed_before_second_phase = `SELECT @@GLOBAL.gtid_executed`

--let $auxiliary_connection = slave
--let $statement_connection = master1
--let $statement = XA ROLLBACK $xid
--let $sync_point = after_call_actions_before_gtid_state_update
--source include/execute_to_conditional_timestamp_sync_point.inc

--source include/rpl/connection_replica.inc
--let $expected_prepared_xa_count = 0
--source common/xa_crash_safe/assert_xa_recover.inc
--error ER_XAER_DUPID
--eval XA START $xid

--source include/rpl/connection_source.inc
--let $source_gtid_executed_after_second_phase = `SELECT @@GLOBAL.gtid_executed`
--let $second_phase_gtid = `SELECT GTID_SUBTRACT('$source_gtid_executed_after_second_phase', '$source_gtid_executed_before_second_phase')`
--let $assert_text = XA second-phase GTID must be non-empty
--let $assert_cond = "$second_phase_gtid" != ""
--source include/assert.inc

--source include/rpl/connection_replica.inc
--source include/dbug_crash_safe.inc

if ($replica_has_binlog == 0) {
  --let $restart_parameters=restart:--skip-replica-start --skip-log-bin --log-replica-updates=0
}
if ($replica_has_binlog == 1) {
  --let $restart_parameters=restart:--skip-replica-start
}
--source include/start_mysqld.inc
--let $rpl_server_number= 2
--source include/rpl/reconnect.inc

--source include/rpl/connection_replica.inc
--let $after_gtid_executed = `SELECT @@GLOBAL.gtid_executed`
--let $assert_text = GTID_EXECUTED has been updated after XA ROLLBACK recovery
--let $assert_cond = "$before_gtid_executed" != "$after_gtid_executed"
--source include/assert.inc
--let $assert_text = GTID_EXECUTED contains the recovered XA ROLLBACK GTID
--let $has_second_phase_gtid = `SELECT GTID_SUBSET('$second_phase_gtid', @@GLOBAL.gtid_executed)`
--let $assert_cond = "$has_second_phase_gtid" = "1"
--source include/assert.inc

--let $expected_prepared_xa_count = 0
--source common/xa_crash_safe/assert_xa_recover.inc

--let $expected_row_count = 2
--source common/xa_crash_safe/assert_row_count.inc

--source include/rpl/start_replica.inc

--source include/rpl/connection_source.inc
DROP TABLE t1;
--source include/rpl/deinit.inc
