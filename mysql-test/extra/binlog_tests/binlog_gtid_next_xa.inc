# ==== Purpose ====
#
# The test is binlog-on-off insensitive to test XA transaction
# identified with explicit GTID_NEXT values.
# It is included from a top-level tests against server
# with --log-bin and --skip-log-bin either.
#
# ==== Usage ====
#
# --let $rpl_skip_log_bin= [1]
#
# Parameters:
#
# $rpl_skip_log_bin
#   top-level caller sets it to 1 when the server is run with --skip-log-bin.
#   The value is checked in the current file in order to pick for execution
#   only one binlog format, as well as passed to interested included files,
#   such as rpl_init.inc.
#
# An XA transaction generates two transactions in the binary log:
#  1. XA BEGIN; DML; XA END; XA PREPARE;
#  2. XA [COMMIT|ROLLBACK];
#
# This test verifies that all possible combinations of GTID_NEXT for
# the two parts of the transaction are allowed (provided that
# GTID_MODE allows it), and that GTID ownership is maintained
# correctly. Specifically, ownership should be released between the
# two transactions and after the two transactions.
#
# ==== Implementation ====
#
# Execute the following:
#
# For COMMIT_OR_ROLLBACK in COMMIT, ROLLBACK:
#   For GTID1 in AUTOMATIC, GTID, ANONYMOUS:
#     For GTID2 in AUTOMATIC, GTID, ANONYMOUS, none:
#       SET GTID_NEXT = <GTID1>;
#       XA BEGIN;
#       DML;
#       XA END;
#       verify that no GTID is owned
#
#       if GTID2 != none:
#         SET GTID_NEXT = <GTID2>;
#       XA <COMMIT_OR_ROLLBACK>;
#       verify that no GTID is owned
#
# The two innermost loops are unrolled because it is not practical to
# implement such logic in mtr.  The body of the inner loop is
# implemented in extra/binlog_tests/{gtid_next_xa,gtid_next_xa_error_simul}.test
# where the latter of two verifies clear exits out of errors, and
# seemless continuation after the server restart while having an XA in
# prepared state.
#
# ==== References ====
#
# WL#6860: binlogging XA transactions
# - Test was created in this worklog.

# No need to run this test in multiple combinations.
if (!$rpl_skip_log_bin)
{
  --source include/have_debug.inc
  --source include/have_binlog_format_statement.inc
  # --log-bin ON specific error simulation
  call mtr.add_suppression("The transaction owned GTID is already in the gtid_executed table");
  CALL mtr.add_suppression("Statement is unsafe because it is being used inside a XA transaction");
}

--let $rpl_gtid_utils= 1
--let $rpl_topology= none
# rpl_init.inc is aware of possible $rpl_skip_log_bin = 1
--source include/rpl_init.inc
--source include/rpl_default_connections.inc
call mtr.add_suppression("Found 1 prepared XA transactions");
--let $rpl_gtid_mode= ON_PERMISSIVE
--source include/rpl_set_gtid_mode.inc

--connect(conn_err_simul,localhost,root,,)

--connection master

CREATE TABLE t1 (a INT);

--let $xid= 1
--let $commit= COMMIT
# This is GTID setting related error
--let $error= 0
# Unlike the above this is an error in applying the prepare or the commit
--let $error_simul =

# Fixme: The declarion has to be done here even though it gets in use only inside
# a sourced file. Otherwise mtr parser reacts buggy near the if() of the following
# while block.
--let $error_at_prepare=0

--let $i= 0
while ($i < 2)
{
  --let $gtid1= AUTOMATIC
  --let $gtid2= AUTOMATIC
  --source extra/binlog_tests/gtid_next_xa.test

  --let $gtid1= AUTOMATIC
  --let $gtid2= $uuida:$xid
  --source extra/binlog_tests/gtid_next_xa.test

  --let $gtid1= AUTOMATIC
  --let $gtid2= ANONYMOUS
  --source extra/binlog_tests/gtid_next_xa.test

  --let $gtid1= AUTOMATIC
  --let $gtid2= none
  --source extra/binlog_tests/gtid_next_xa.test

  --let $gtid1= $uuida:$xid
  --let $gtid2= AUTOMATIC
  --source extra/binlog_tests/gtid_next_xa.test

  --let $gtid1= $uuida:$xid
  --let $gtid2= $uuidb:$xid
  --source extra/binlog_tests/gtid_next_xa.test

  --let $gtid1= $uuida:$xid
  --let $gtid2= ANONYMOUS
  --source extra/binlog_tests/gtid_next_xa.test

  --let $error= convert_error(ER_GTID_NEXT_TYPE_UNDEFINED_GTID)
  --let $gtid1= $uuida:$xid
  --let $gtid2= none
  --source extra/binlog_tests/gtid_next_xa.test
  --let $error= 0

  --let $gtid1= ANONYMOUS
  --let $gtid2= AUTOMATIC
  --source extra/binlog_tests/gtid_next_xa.test

  --let $gtid1= ANONYMOUS
  --let $gtid2= $uuida:$xid
  --source extra/binlog_tests/gtid_next_xa.test

  --let $gtid1= ANONYMOUS
  --let $gtid2= ANONYMOUS
  --source extra/binlog_tests/gtid_next_xa.test

  --let $gtid1= ANONYMOUS
  --let $gtid2= none
  --source extra/binlog_tests/gtid_next_xa.test

  # Commit 1 phase or Rollback  "clean" case
  --let $one_phase = one phase
  --let $gtid1= $uuida:$xid
  --let $gtid2= $uuidb:$xid
  --source extra/binlog_tests/gtid_next_xa.test
  --let $one_phase =

  # Commit 1 phase or Rollback simulated error case
  --let $one_phase = one phase
  --let $error_simul = at_commit
  --let $gtid1= $uuida:$xid
  --let $gtid2= $uuidb:$xid
  --source extra/binlog_tests/gtid_next_xa_error_simul.test
  --let $error_simul=
  --let $one_phase =

  # Regular 2 phase cases with simulated error

  --let $error_simul = at_prepare
  --let $gtid1= $uuida:$xid
  --let $gtid2= $uuidb:$xid
  --source extra/binlog_tests/gtid_next_xa_error_simul.test
  --let $error_simul=

  --let $error_simul = at_commit
  --let $gtid1= $uuida:$xid
  --let $gtid2= $uuidb:$xid
  --source extra/binlog_tests/gtid_next_xa_error_simul.test
  --let $error_simul=

  --let $do_shutdown_after_prepare = 1
  --let $error_simul = at_commit
  --let $gtid1= $uuida:$xid
  --let $gtid2= $uuidb:$xid
  --source extra/binlog_tests/gtid_next_xa_error_simul.test
  --let $error_simul=
  --let $do_shutdown_after_prepare = 0

  --inc $i
  --let $commit= ROLLBACK
}

DROP TABLE t1;

# rpl_init.inc saves GTID_MODE in the $gtid_mode variable
--let $rpl_gtid_mode= $gtid_mode
--source include/rpl_set_gtid_mode.inc

--source include/rpl_end.inc
