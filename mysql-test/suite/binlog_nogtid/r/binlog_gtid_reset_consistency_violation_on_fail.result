# TC1. Test if GTID consistency state is rolled back after failure
# ----------------------------------------------------------------
XA START 'xa1';
XA END 'xa1';
XA PREPARE 'xa1';
SET DEBUG_SYNC='restore_previous_state_after_statement_failed SIGNAL failure';
CREATE TEMPORARY TABLE t1 (c INT) ENGINE=InnoDB;
ERROR XAE07: XAER_RMFAIL: The command cannot be executed when global transaction is in the  PREPARED state
SET DEBUG_SYNC='now WAIT_FOR failure';
SET @@SESSION.GTID_NEXT='ANONYMOUS';
XA COMMIT 'xa1';
# TC2. Test if GTID consistency state is persisted after success
# --------------------------------------------------------------
SET DEBUG_SYNC='persist_new_state_after_statement_succeeded SIGNAL success';
CREATE TEMPORARY TABLE t1 (c INT) ENGINE=InnoDB;
SET DEBUG_SYNC='now WAIT_FOR success';
DROP TABLE t1;
SET DEBUG_SYNC='RESET';
