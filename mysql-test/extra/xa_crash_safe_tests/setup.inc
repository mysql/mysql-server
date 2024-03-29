# ==== Purpose ====
#
# Used by scripts testing XA crash recovery, it sets up the scenario
# requirements, for instance, creates table and inserts records, sets up
# GR, initializes constants used in the tests, etc.
#
# ==== Parameters ====
#
# $xid
#   The XID of the XA transaction that is going to be used in the test.
#
# ==== References ====
#
# Related tests:
#
# - mysql-test/t/xa_commit_failure_before_commit_engines.test
# - mysql-test/t/xa_commit_failure_before_commit_tc.test
# - mysql-test/t/xa_commit_failure_before_gtid_externalization.test
# - mysql-test/t/xa_commit_one_phase_failure_before_commit_engines.test
# - mysql-test/t/xa_commit_one_phase_failure_before_commit_tc.test
# - mysql-test/t/xa_commit_one_phase_failure_before_gtid_externalization.test
# - mysql-test/t/xa_prepare_failure_before_gtid_externalization.test
# - mysql-test/t/xa_prepare_failure_before_prepare_2nd_phase.test
# - mysql-test/t/xa_prepare_failure_before_prepare_in_engines.test
# - mysql-test/t/xa_prepare_failure_before_prepare_tc.test
# - mysql-test/t/xa_rollback_failure_before_gtid_externalization.test
# - mysql-test/t/xa_rollback_failure_before_rollback_engines.test
# - mysql-test/t/xa_rollback_failure_before_rollback_tc.test
#
# - mysql-test/suite/binlog/t/binlog_xa_recovery_errors.test
#
# - mysql-test/suite/binlog_gtid/t/binlog_gtid_xa_multiple_failures.test
# - mysql-test/suite/binlog_gtid/t/binlog_gtid_binlog_recovery_errors.test
# - mysql-test/suite/binlog_gtid/t/binlog_gtid_xa_commit_failure_after_rotate.test
# - mysql-test/suite/binlog_gtid/t/binlog_gtid_xa_commit_failure_before_commit_engines.test
# - mysql-test/suite/binlog_gtid/t/binlog_gtid_xa_commit_failure_before_gtid_externalization.test
# - mysql-test/suite/binlog_gtid/t/binlog_gtid_xa_commit_failure_before_write_binlog.test
# - mysql-test/suite/binlog_gtid/t/binlog_gtid_xa_commit_one_phase_failure_before_commit_engines.test
# - mysql-test/suite/binlog_gtid/t/binlog_gtid_xa_commit_one_phase_failure_before_gtid_externalizatio .test
# - mysql-test/suite/binlog_gtid/t/binlog_gtid_xa_commit_one_phase_failure_before_prepare_in_engines.test
# - mysql-test/suite/binlog_gtid/t/binlog_gtid_xa_commit_one_phase_failure_before_write_binlog.test
# - mysql-test/suite/binlog_gtid/t/binlog_gtid_xa_prepare_failure_before_gtid_externalization.test
# - mysql-test/suite/binlog_gtid/t/binlog_gtid_xa_prepare_failure_before_prepare_2nd_phase.test
# - mysql-test/suite/binlog_gtid/t/binlog_gtid_xa_prepare_failure_before_prepare_in_engines.test
# - mysql-test/suite/binlog_gtid/t/binlog_gtid_xa_prepare_failure_before_write_binlog.test
# - mysql-test/suite/binlog_gtid/t/binlog_gtid_xa_rollback_failure_after_rotate.test
# - mysql-test/suite/binlog_gtid/t/binlog_gtid_xa_rollback_failure_before_gtid_externalization.test
# - mysql-test/suite/binlog_gtid/t/binlog_gtid_xa_rollback_failure_before_rollback_engines.test
# - mysql-test/suite/binlog_gtid/t/binlog_gtid_xa_rollback_failure_before_write_binlog.test
#
# - mysql-test/suite/group_replication/t/gr_xa_commit_failure_after_rotate.test
# - mysql-test/suite/group_replication/t/gr_xa_commit_failure_before_broadcast.test
# - mysql-test/suite/group_replication/t/gr_xa_commit_failure_before_commit_engines.test
# - mysql-test/suite/group_replication/t/gr_xa_commit_failure_before_gtid_externalization.test
# - mysql-test/suite/group_replication/t/gr_xa_commit_failure_before_write_binlog.test
# - mysql-test/suite/group_replication/t/gr_xa_commit_one_phase_failure_before_broadcast.test
# - mysql-test/suite/group_replication/t/gr_xa_commit_one_phase_failure_before_commit_engines.test
# - mysql-test/suite/group_replication/t/gr_xa_commit_one_phase_failure_before_gtid_externalization.test
# - mysql-test/suite/group_replication/t/gr_xa_commit_one_phase_failure_before_prepare_in_engines.tes
# - mysql-test/suite/group_replication/t/gr_xa_commit_one_phase_failure_before_write_binlog.test
# - mysql-test/suite/group_replication/t/gr_xa_prepare_failure_before_broadcast.test
# - mysql-test/suite/group_replication/t/gr_xa_prepare_failure_before_gtid_externalization.test
# - mysql-test/suite/group_replication/t/gr_xa_prepare_failure_before_prepare_2nd_phase.test
# - mysql-test/suite/group_replication/t/gr_xa_prepare_failure_before_prepare_in_engines.test
# - mysql-test/suite/group_replication/t/gr_xa_prepare_failure_before_write_binlog.test
# - mysql-test/suite/group_replication/t/gr_xa_rollback_failure_after_rotate.test
# - mysql-test/suite/group_replication/t/gr_xa_rollback_failure_before_broadcast.test
# - mysql-test/suite/group_replication/t/gr_xa_rollback_failure_before_gtid_externalization.test
# - mysql-test/suite/group_replication/t/gr_xa_rollback_failure_before_rollback_engines.test
# - mysql-test/suite/group_replication/t/gr_xa_rollback_failure_before_write_binlog.test
#
# - mysql-test/suite/rpl_gtid/t/rpl_gtid_xa_multiple_failures.test
# - mysql-test/suite/rpl_gtid/t/rpl_gtid_xa_commit_failure_after_rotate.test
# - mysql-test/suite/rpl_gtid/t/rpl_gtid_xa_commit_failure_before_commit_engines.test
# - mysql-test/suite/rpl_gtid/t/rpl_gtid_xa_commit_failure_before_gtid_externalization.test
# - mysql-test/suite/rpl_gtid/t/rpl_gtid_xa_commit_failure_before_write_binlog.test
# - mysql-test/suite/rpl_gtid/t/rpl_gtid_xa_commit_one_phase_failure_before_commit_engines.test
# - mysql-test/suite/rpl_gtid/t/rpl_gtid_xa_commit_one_phase_failure_before_gtid_externalization.test
# - mysql-test/suite/rpl_gtid/t/rpl_gtid_xa_commit_one_phase_failure_before_prepare_in_engines.test
# - mysql-test/suite/rpl_gtid/t/rpl_gtid_xa_commit_one_phase_failure_before_write_binlog.test
# - mysql-test/suite/rpl_gtid/t/rpl_gtid_xa_prepare_failure_before_gtid_externalization.test
# - mysql-test/suite/rpl_gtid/t/rpl_gtid_xa_prepare_failure_before_prepare_2nd_phase.test
# - mysql-test/suite/rpl_gtid/t/rpl_gtid_xa_prepare_failure_before_prepare_in_engines.test
# - mysql-test/suite/rpl_gtid/t/rpl_gtid_xa_prepare_failure_before_write_binlog.test
# - mysql-test/suite/rpl_gtid/t/rpl_gtid_xa_rollback_failure_after_rotate.test
# - mysql-test/suite/rpl_gtid/t/rpl_gtid_xa_rollback_failure_before_gtid_externalization.test
# - mysql-test/suite/rpl_gtid/t/rpl_gtid_xa_rollback_failure_before_rollback_engines.test
# - mysql-test/suite/rpl_gtid/t/rpl_gtid_xa_rollback_failure_before_write_binlog.test
#

if ($xid == '') {
  --die ERROR IN TEST: You must set $xid before sourcing xa_crash_safe_testing_setup.inc
}

# Add suppression for XA transaction related warning
let $messages = Found .* prepared XA transactions
.*Checksum mismatch in datafile.*;
--source include/suppress_messages.inc

# If in the GR setup
if ($rpl_group_replication == 1) {
  --let $rpl_connection_name = server1
  --source include/rpl_connection.inc

  --source include/start_and_bootstrap_group_replication.inc
  --let $_group_replication_local_address = `SELECT @@GLOBAL.group_replication_local_address`
  --let $_group_replication_group_seeds = `SELECT @@GLOBAL.group_replication_group_seeds`
  --let $_group_replication_group_name = `SELECT @@GLOBAL.group_replication_group_name`
}

# Create the table and insert records
CREATE TABLE t1 (c1 INT PRIMARY KEY);

--eval XA START $xid
INSERT INTO t1 VALUES (2);
--eval XA END $xid
--eval XA PREPARE $xid
--eval XA COMMIT $xid

# Allow for `save_binlog_position.inc` to save the binlog info in the
# connection session in order for the tests to be able to use
# `assert_binlog_events.inc` for more than one server.
--let $save_binlog_position_in_sql = 1

if ($rpl_group_replication == 1) {
  # Bootstrap server 2
  --let $rpl_connection_name = server2
  --source include/rpl_connection.inc
  --let $sysvars_to_save = [ "GLOBAL.group_replication_member_expel_timeout" ]
  --source include/save_sysvars.inc
  SET GLOBAL group_replication_member_expel_timeout = 0;
  --source include/start_group_replication.inc
  # Save binlog position for server 2
  --source include/save_binlog_position.inc

  # Bootstrap server 2
  --let $rpl_connection_name = server3
  --source include/rpl_connection.inc
  --let $sysvars_to_save = [ "GLOBAL.group_replication_member_expel_timeout" ]
  --source include/save_sysvars.inc
  SET GLOBAL group_replication_member_expel_timeout = 0;
  --source include/start_group_replication.inc

  # Save binlog position for server 1
  --let $rpl_connection_name = server1
  --source include/rpl_connection.inc
  --source include/save_binlog_position.inc
}

# If not in GR mode
if ($rpl_group_replication != 1) {
  # If not in source->replica mode
  if ($rpl_inited) {
    --source include/sync_slave_sql_with_master.inc
  }
  if (`SELECT @@GLOBAL.log_bin != 0`) {
    # Save binlog position for the current server
    --source include/save_binlog_position.inc
  }
  if ($rpl_inited) {
    --source include/rpl_connection_master.inc
  }
}

# Initialize constants to be used with `assert_binlog_events.inc`
--let $empty_event_sequence = ()
--let $view_change = Gtid # Query/BEGIN # View_change # Query/COMMIT
--let $xa_start_end = Query/XA START $xid # Table_map # Write_rows # Query/XA END $xid
