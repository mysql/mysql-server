--source include/have_debug.inc

################################################################################
# WL929 - CHECK CONSTRAINTS.                                                   #
#         Test cases to verify MDL on check constraints.                       #
################################################################################

--enable_connect_log

--echo #------------------------------------------------------------------------
--echo # Test case to verify MDL locking on check constraints with same names
--echo # in the concurrent CREATE TABLE statements.
--echo #------------------------------------------------------------------------
SET DEBUG_SYNC="after_acquiring_lock_on_check_constraints SIGNAL cc_locked WAIT_FOR continue";
--SEND CREATE TABLE t1 (f1 INT CHECK (f1 < 10), f2 INT, CONSTRAINT t1_ck CHECK(f2 < 10));

CONNECT (con1, localhost, root);
SET DEBUG_SYNC="now WAIT_FOR cc_locked";
--send CREATE TABLE t2 (f1 INT, f2 INT, CONSTRAINT t1_ck CHECK(f2 < 10));

CONNECT (con2, localhost, root);
--echo # default connection acquires MDL lock on the check constraint name 'test.t1_ck'.
--echo # con1 waits for the MDL lock on 'test.t1_ck' at this point.
let $wait_condition=
  SELECT count(*) = 1 FROM INFORMATION_SCHEMA.PROCESSLIST
  WHERE state = "Waiting for check constraint metadata lock" AND
        info  = "CREATE TABLE t2 (f1 INT, f2 INT, CONSTRAINT t1_ck CHECK(f2 < 10))";
--source include/wait_condition.inc
SET DEBUG_SYNC="now SIGNAL continue";

CONNECTION con1;
--error ER_CHECK_CONSTRAINT_DUP_NAME
--reap

CONNECTION default;
--reap


--echo #------------------------------------------------------------------------
--echo # Test case to verify MDL locking on check constraints names in the
--echo # RENAME TABLE and CREATE TABLE statements.
--echo #------------------------------------------------------------------------
SET DEBUG_SYNC="after_acquiring_lock_on_check_constraints_for_rename SIGNAL cc_locked WAIT_FOR continue";
--SEND RENAME TABLE t1 to t2;

CONNECTION con1;
SET DEBUG_SYNC="now WAIT_FOR cc_locked";
--send CREATE TABLE t3 (f1 INT, CONSTRAINT t1_chk_1 CHECK (f1 < 10));

CONNECTION con2;
--echo # default connection acquires lock on check constraint 'test.t1_chk_1'.
--echo # Concurrent create operation with same name for check constraint in con1
--echo # waits for the lock.
let $wait_condition=
  SELECT count(*) = 1 FROM INFORMATION_SCHEMA.PROCESSLIST
  WHERE state = "Waiting for check constraint metadata lock" AND
        info  = "CREATE TABLE t3 (f1 INT, CONSTRAINT t1_chk_1 CHECK (f1 < 10))";
--source include/wait_condition.inc
SET DEBUG_SYNC="now SIGNAL continue";

CONNECTION con1;
--reap

CONNECTION default;
--reap
SHOW CREATE TABLE t2;
SHOW CREATE TABLE t3;
DROP TABLE t3;


--echo #------------------------------------------------------------------------
--echo # Test case to verify MDL locking on generated check constraints names
--echo # in the RENAME TABLE using the target table name and CREATE TABLE
--echo # statements.
--echo #------------------------------------------------------------------------
SET DEBUG_SYNC="after_acquiring_lock_on_check_constraints_for_rename SIGNAL cc_locked WAIT_FOR continue";
--SEND RENAME TABLE t2 to t1;

CONNECTION con1;
SET DEBUG_SYNC="now WAIT_FOR cc_locked";
--send CREATE TABLE t3 (f1 INT, CONSTRAINT t1_chk_1 CHECK (f1 < 10));

CONNECTION con2;
--echo # default connection acquires lock on check constraint name('test.t1_chk_1')
--echo # generated using target table t1.
--echo # concurrent con1 waits for the MDL on test.t1_chk_1 in CREATE TABLE
--echo # statement.
let $wait_condition=
  SELECT count(*) = 1 FROM INFORMATION_SCHEMA.PROCESSLIST
  WHERE state = "Waiting for check constraint metadata lock" AND
        info  = "CREATE TABLE t3 (f1 INT, CONSTRAINT t1_chk_1 CHECK (f1 < 10))";
--source include/wait_condition.inc
SET DEBUG_SYNC="now SIGNAL continue";

CONNECTION con1;
--error ER_CHECK_CONSTRAINT_DUP_NAME
--reap
SHOW CREATE TABLE t1;

CONNECTION default;
--reap


--echo #------------------------------------------------------------------------
--echo # Test case to verify MDL locking on check constraint name in ALTER
--echo # TABLE statement to RENAME table and CREATE TABLE statements.
--echo #------------------------------------------------------------------------
SET DEBUG_SYNC="after_acquiring_lock_on_check_constraints_for_rename SIGNAL cc_locked WAIT_FOR continue";
--SEND ALTER TABLE t1 RENAME TO t3;

CONNECTION con1;
SET DEBUG_SYNC="now WAIT_FOR cc_locked";
--send CREATE TABLE t2 (f1 INT, CONSTRAINT t1_chk_1 CHECK (f1 < 10));

CONNECTION con2;
--echo # default connection acquires lock on check constraint 'test.t1_chk_1'.
--echo # Concurrent con1 waits for lock on test.t1_chk_1.
let $wait_condition=
  SELECT count(*) = 1 FROM INFORMATION_SCHEMA.PROCESSLIST
  WHERE state = "Waiting for check constraint metadata lock" AND
        info  = "CREATE TABLE t2 (f1 INT, CONSTRAINT t1_chk_1 CHECK (f1 < 10))";
--source include/wait_condition.inc
SET DEBUG_SYNC="now SIGNAL continue";

CONNECTION con1;
--reap

CONNECTION default;
--reap
SHOW CREATE TABLE t2;
SHOW CREATE TABLE t3;
DROP TABLE t2;


--echo #------------------------------------------------------------------------
--echo # Test case to verify MDL locking on generated check constraint name
--echo # using target table name in ALTER TABLE statement to RENAME table and
--echo # CREATE TABLE statements.
--echo #------------------------------------------------------------------------
SET DEBUG_SYNC="after_acquiring_lock_on_check_constraints_for_rename SIGNAL cc_locked WAIT_FOR continue";
--SEND ALTER TABLE t3 RENAME TO t1;

CONNECTION con1;
SET DEBUG_SYNC="now WAIT_FOR cc_locked";
--send CREATE TABLE t2 (f1 INT, CONSTRAINT t1_chk_1 CHECK (f1 < 10));

CONNECTION con2;
--echo # default connection acquires lock on the generated check constraint
--echo # name('test.t1_chk_1') using target table name t1. con1 waits for
--echo # the lock on same name for check constraint.
let $wait_condition=
  SELECT count(*) = 1 FROM INFORMATION_SCHEMA.PROCESSLIST
  WHERE state = "Waiting for check constraint metadata lock" AND
        info  = "CREATE TABLE t2 (f1 INT, CONSTRAINT t1_chk_1 CHECK (f1 < 10))";
--source include/wait_condition.inc
SET DEBUG_SYNC="now SIGNAL continue";

CONNECTION con1;
--error ER_CHECK_CONSTRAINT_DUP_NAME
--reap

CONNECTION default;
--reap
SHOW CREATE TABLE t1;


--echo #------------------------------------------------------------------------
--echo # Test case to verify check constraint evaluation skip for unaffected
--echo # columns during update operation.
--echo #------------------------------------------------------------------------
CONNECTION default;
SET @binlog_format_saved = @@binlog_format;
SET binlog_format = 'STATEMENT';
CREATE TABLE t2 (f1 INT, f2 INT, f3 INT CONSTRAINT f3_ck CHECK(f3 < 10));
SHOW CREATE TABLE t2;
INSERT INTO t2 VALUES (5, 5, 5);
SELECT * FROM t2;
SET DEBUG_SYNC="skip_check_constraints_on_unaffected_columns SIGNAL check_proc_state WAIT_FOR continue";
--send UPDATE t2 SET f2 = 10 WHERE f1 = 5;

CONNECTION con1;
--echo # Column f2 is unaffected and it is not used in building a row during
--echo # update operation. So check constraint f3_ck is not evaluated.
--echo # DEBUG_SYNC is hit on skipping a check constraint.
SET DEBUG_SYNC="now WAIT_FOR check_proc_state";
let $wait_condition=
  SELECT count(*) = 1 FROM INFORMATION_SCHEMA.PROCESSLIST
                      WHERE info = "UPDATE t2 SET f2 = 10 WHERE f1 = 5" AND
                            state LIKE '%skip_check_constraints_on_unaffected_columns%';
--source include/wait_condition.inc
SET DEBUG_SYNC="now SIGNAL continue";

CONNECTION default;
--reap
SELECT * FROM t2;
SET binlog_format=@binlog_format_saved;

#Cleanup
CONNECTION default;
DISCONNECT con1;
DISCONNECT con2;
SET DEBUG_SYNC='RESET';
DROP TABLE t1, t2;
--disable_connect_log


--echo #
--echo # Bug#32018406 - CREATE TABLE MAY ASSERT IN DEBUG MODE
--echo #
--let trace_file= $MYSQL_TMP_DIR/bug32018406.trace
--let debug_options= '+d,info:O,$trace_file'
--replace_result $debug_options '+d,info:O,trace_file'
--eval SET SESSION DEBUG= $debug_options

# Without fix, following statements crash in debug mode.
CREATE TABLE t1 (c1 INT, c2 INT CHECK (c2 < 100));
ALTER TABLE t1 DROP COLUMN c2;
DROP TABLE t1;

#Cleanup
SET SESSION debug= '-d,info:-O';
--remove_file $trace_file
