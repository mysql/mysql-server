# TC1. Test if GTID consistency state is rolled back after failure
# ----------------------------------------------------------------
SET DEBUG_SYNC='restore_previous_state_after_statement_failed SIGNAL failure';
BEGIN;
DROP TEMPORARY TABLE t1;
ERROR 42S02: Unknown table 'test.t1'
SET DEBUG_SYNC='now WAIT_FOR failure';
ROLLBACK;
# TC2. Test if GTID consistency state is persisted after success
# --------------------------------------------------------------
SET DEBUG_SYNC='persist_new_state_after_statement_succeeded SIGNAL success';
CREATE TEMPORARY TABLE t1 (c INT) ENGINE=InnoDB;
SET DEBUG_SYNC='now WAIT_FOR success';
DROP TABLE t1;
SET DEBUG_SYNC='RESET';
