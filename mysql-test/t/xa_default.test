#
# This test is a "port" of the basic xa functionality test xa.test modified and
# adapted to run with xa_detach_on_prepare=on. It also includes a few testcases
# specific to this mode, e.g. making sure it is possible to start new
# transactions (normal or XA) after XA PREPARE, and that it is possible to pick
# up an XA transaction from a connection which is still active.

# Test requires --xa_detach_on_prepare
--let $option_name = xa_detach_on_prepare
--let $option_value = 1
--source include/only_with_option.inc

# Save the initial number of concurrent sessions
--source include/count_sessions.inc

--echo # Make it easier to trace connections changes
--enable_connect_log

--echo # Simple rollback after prepare
CREATE TABLE t1 (a INT);
XA START 'test1';
INSERT INTO t1 VALUES (10);
XA END 'test1';
XA PREPARE 'test1';
XA ROLLBACK 'test1';
SELECT * FROM t1;

--echo # Verify that new XA trx cannot be started before XA PREPARE
XA START 'test2';
--error ER_XAER_RMFAIL
XA START 'test-bad';
INSERT INTO t1 VALUES (20);
--error ER_XAER_RMFAIL
XA PREPARE 'test2';
XA END 'test2';
XA PREPARE 'test2';
XA COMMIT 'test2';
SELECT * FROM t1;

--echo # Verify that COMMIT, BEGIN and CREATE TABLE is disallowed inside XA trx.
XA START 'testa','testb';
INSERT INTO t1 VALUES (30);

--error ER_XAER_RMFAIL
COMMIT;

XA END 'testa','testb';

--error ER_XAER_RMFAIL
BEGIN;
--error ER_XAER_RMFAIL
CREATE TABLE t2 (a INT);

--echo # Test parallel XA trx on a different connection
--connect (con1,localhost,root,,)

--error ER_XAER_DUPID
XA START 'testa','testb';
--error ER_XAER_DUPID
XA START 'testa','testb', 123;

#        gtrid [ , bqual [ , formatID ] ]
XA START 0x7465737462, 0x2030405060, 0xb;
INSERT INTO t1 VALUES (40);
XA END 'testb',' 0@P`',11;
XA PREPARE 'testb',0x2030405060,11;

--echo # Show that it is possible to start new transaction after XA PREPARE
START TRANSACTION;
INSERT INTO t1 VALUES (42);
COMMIT WORK;

--sorted_result
XA RECOVER;

--connection default

XA PREPARE 'testa','testb';

--sorted_result
XA RECOVER;

--echo # Can commit prepared XA trx from other connection
XA COMMIT 'testb',0x2030405060,11;

XA ROLLBACK 'testa','testb';

--echo # XA trx from other connection not found (already committed)
--error ER_XAER_NOTA
XA COMMIT 'testb',0x2030405060,11;

SELECT * FROM t1;

DROP TABLE t1;

--connection con1
--disconnect con1
--source include/wait_until_disconnected.inc
--connection default


#
# Bug#28323: Server crashed in xid cache operations
#

CREATE TABLE t1(a INT, b INT, c VARCHAR(20), PRIMARY KEY(a));
INSERT INTO t1 VALUES (1, 1, 'a');
INSERT INTO t1 VALUES (2, 2, 'b');

--connect (con1,localhost,root,,)
--connect (con2,localhost,root,,)

--connection con1
XA START 'a','b';
UPDATE t1 SET c = 'aa' WHERE a = 1;
--connection con2
XA START 'a','c';
UPDATE t1 SET c = 'bb' WHERE a = 2;
--connection con1
--send UPDATE t1 SET c = 'bb' WHERE a = 2
--connection con2
--sleep 1
--error ER_LOCK_DEADLOCK
UPDATE t1 SET c = 'aa' WHERE a = 1;
--error ER_XA_RBDEADLOCK
SELECT COUNT(*) FROM t1;
--error ER_XA_RBDEADLOCK
XA END 'a','c';
XA ROLLBACK 'a','c';
--disconnect con2

--connect (con3,localhost,root,,)
--connection con3
XA START 'a','c';
--connection con1
--reap
--disconnect con1
--disconnect con3
--connection default
DROP TABLE t1;

--echo #
--echo # BUG#51342 - more xid crashing
--echo #
CREATE TABLE t1(a INT);
XA START 'x';
SET SESSION autocommit=0;
INSERT INTO t1 VALUES(1);
--error ER_XAER_RMFAIL
SET SESSION autocommit=1;
SELECT @@autocommit;
INSERT INTO t1 VALUES(1);
XA END 'x';
XA COMMIT 'x' ONE PHASE;
DROP TABLE t1;
SET SESSION autocommit=1;

#
# Bug#44672: Assertion failed: thd->transaction.xid_state.xid.is_null()
#

XA START 'a';
XA END 'a';
XA ROLLBACK 'a';
XA START 'a';
XA END 'a';
XA ROLLBACK 'a';

#
# Bug#45548: XA transaction without access to InnoDB tables crashes the server
#

XA START 'a';
XA END 'a';
XA PREPARE 'a';
XA COMMIT 'a';

XA START 'a';
XA END 'a';
XA PREPARE 'a';
XA COMMIT 'a';

#
# BUG#43171 - Assertion failed: thd->transaction.xid_state.xid.is_null()
#
CREATE TABLE t1(a INT, KEY(a));
# We insert 100 rows into t1 as otherwise the hypergraph optimizer chooses 
# table scans for the update statements resulting in deadlocks.
INSERT INTO t1
WITH RECURSIVE t(i) AS (
    SELECT 0 AS i UNION ALL
    SELECT i + 1 FROM t WHERE i + 1 < 100
)
SELECT i FROM t;
ANALYZE TABLE t1;
--connect(con1,localhost,root,,)

# Part 1: Prepare to test XA START after regular transaction deadlock
BEGIN;
UPDATE t1 SET a=3 WHERE a=1;

--connection default
BEGIN;
UPDATE t1 SET a=4 WHERE a=2;

--connection con1
let $conn_id= `SELECT CONNECTION_ID()`;
SEND UPDATE t1 SET a=5 WHERE a=2;

# Wait until the UPDATE statement has started executing. The old
# optimizer uses the single-table UPDATE code path (which sets state
# to 'Searching rows for update'), whereas the hypergraph optimizer
# uses a common code path for single-table and multi-table UPDATE and
# sets the state to 'executing'.
--connection default
let $wait_timeout= 2;
let $wait_condition= SELECT 1 FROM INFORMATION_SCHEMA.PROCESSLIST
WHERE ID=$conn_id AND STATE IN ('Searching rows for update', 'executing');
--source include/wait_condition.inc

--error ER_LOCK_DEADLOCK
UPDATE t1 SET a=5 WHERE a=1;
ROLLBACK;

# Part 2: Prepare to test XA START after XA transaction deadlock
--connection con1
REAP;
ROLLBACK;
BEGIN;
UPDATE t1 SET a=3 WHERE a=1;

--connection default
XA START 'xid1';
UPDATE t1 SET a=4 WHERE a=2;

--connection con1
SEND UPDATE t1 SET a=5 WHERE a=2;

--connection default
let $wait_timeout= 2;
let $wait_condition= SELECT 1 FROM INFORMATION_SCHEMA.PROCESSLIST
WHERE ID=$conn_id AND STATE IN ('Searching rows for update', 'executing');
--source include/wait_condition.inc

--error ER_LOCK_DEADLOCK
UPDATE t1 SET a=5 WHERE a=1;
--error ER_XA_RBDEADLOCK
XA END 'xid1';
XA ROLLBACK 'xid1';

XA START 'xid1';
XA END 'xid1';
XA ROLLBACK 'xid1';

--connection con1
REAP;
--disconnect con1

--connection default
DROP TABLE t1;


--echo #
--echo # Bug#56448 Assertion failed: ! is_set() with second xa end
--echo #

XA START 'x';
XA END 'x';
# Second XA END caused an assertion.
--error ER_XAER_RMFAIL
XA END 'x';
XA PREPARE 'x';
# Second XA PREPARE also caused an assertion.
--error ER_XAER_RMFAIL
XA PREPARE 'x';
XA ROLLBACK 'x';


--echo #
--echo # Bug#59986 Assert in Diagnostics_area::set_ok_status() for XA COMMIT
--echo #

CREATE TABLE t1(a INT, b INT, PRIMARY KEY(a));
INSERT INTO t1 VALUES (1, 1), (2, 2);

--connect (con1, localhost, root)
XA START 'a';
UPDATE t1 SET b= 3 WHERE a=1;

--connection default
XA START 'b';
UPDATE t1 SET b=4 WHERE a=2;
--echo # Sending:
--send UPDATE t1 SET b=5 WHERE a=1

--connection con1
--sleep 1
--error ER_LOCK_DEADLOCK
UPDATE t1 SET b=6 WHERE a=2;
# This used to trigger the assert
--error ER_XA_RBDEADLOCK
XA COMMIT 'a';

--connection default
--echo # Reaping: UPDATE t1 SET b=5 WHERE a=1
--reap
XA END 'b';
XA ROLLBACK 'b';
DROP TABLE t1;
--disconnect con1


--echo #
--echo # Bug#11766752 59936: multiple xa assertions - transactional
--echo #              statement fuzzer
--echo #

CREATE TABLE t1 (a INT);
XA START 'a';
INSERT INTO t1 VALUES (1);

SAVEPOINT savep;

XA END 'a';
--error ER_XAER_RMFAIL
SELECT * FROM t1;
--error ER_XAER_RMFAIL
INSERT INTO t1 VALUES (2);
--error ER_XAER_RMFAIL
SAVEPOINT savep;
--error ER_XAER_RMFAIL
SET @a=(SELECT * FROM t1);

XA PREPARE 'a';
SELECT * FROM t1;
INSERT INTO t1 VALUES (2);
SAVEPOINT savep;
#SET @a=(SELECT * FROM t1);
#UPDATE t1 SET a=1 WHERE a=2;

XA COMMIT 'a';
SELECT * FROM t1;
--error ER_SUBQUERY_NO_1_ROW
SET @a=(SELECT * FROM t1);
UPDATE t1 SET a=1 WHERE a=2;
DROP TABLE t1;


--echo #
--echo # Bug#12352846 - TRANS_XA_START(THD*):
--echo #                ASSERTION THD->TRANSACTION.XID_STATE.XID.IS_NULL()
--echo #                FAILED
--echo #

# We need to create a deadlock in which xa transaction will be chosen as
# a victim and rolled back. We will use this scenario:
# 1. connection default obtains LOCK_X on t1 record
# 2. connection con2 obtains LOCK_X on t2 and waits for LOCK_X on t1
# 3. connection default tries to obtain LOCK_X on t2 which causes
#    a cycle, which is resolved by choosing con2 as a victim, because
#    default is "heavier" due to writes made in t1

CREATE TABLE t1 (a INT) ENGINE=InnoDB;
INSERT INTO t1 VALUES (1);
CREATE TABLE t2 (a INT) ENGINE=InnoDB;
INSERT INTO t2 VALUES (1);

START TRANSACTION;

# Step 1.
DELETE FROM t1;

--connect (con2,localhost,root)
XA START 'xid1';
--echo # Sending:

# Step 2.
SELECT a FROM t2 WHERE a=1 FOR SHARE;
--send SELECT a FROM t1 WHERE a=1 FOR UPDATE

--connection default
--echo # Waiting for until a transaction with 'SELECT...FOR UPDATE'
--echo # will be locked inside innodb subsystem.

let $wait_condition=
  SELECT COUNT(*) = 1 FROM information_schema.innodb_trx
  WHERE trx_query = 'SELECT a FROM t1 WHERE a=1 FOR UPDATE' AND
  trx_operation_state = 'starting index read' AND
  trx_state = 'LOCK WAIT';
--source include/wait_condition.inc

# Step 3.
SELECT a FROM t2 WHERE a=1 FOR UPDATE;

--connection con2
--echo # Reaping: SELECT a FROM t1 WHERE a=1 FOR UPDATE
--error ER_LOCK_DEADLOCK
--reap
--error ER_XA_RBDEADLOCK
XA COMMIT 'xid1';

--connection default

COMMIT;

--connection con2
# This caused the assert to be triggered
XA START 'xid1';

XA END 'xid1';
XA PREPARE 'xid1';
XA ROLLBACK 'xid1';

--connection default
DROP TABLE t1;
DROP TABLE t2;
--disconnect con2


# Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc

--echo #
--echo # Bug#14670465 PLEASE PRINT HUMAN READABLE, ESCAPED
--echo #              XID DATA IN XA RECOVER OUTPUT
--echo #
--echo #
--echo # xa Recover command was not diplaying non printable ASCII
--echo # characters in the XID previosuly. Now there is another column
--echo # in the result set which is a Hex Encoded String of the XID.
--echo #

--echo # Check that XIDs which are not normally printable are displayed
--echo # in readable format when CONVERT XID clause is used.
XA START 0xABCDEF1234567890, 0x01, 0x02 ;
XA END 0xABCDEF1234567890, 0x01, 0x02 ;
XA PREPARE 0xABCDEF1234567890, 0x01, 0x02 ;
--sorted_result
XA RECOVER convert xid;
XA ROLLBACK 0xABCDEF1234567890, 0x01, 0x02 ;

--echo # Check that XID which has only printable characters are displayed
--echo # correctly without using of CONVERT XID clause
XA START 0x4142434445, 0x46, 0x02 ;
XA END 0x4142434445, 0x46, 0x02 ;
XA PREPARE 0x4142434445, 0x46, 0x02 ;
--sorted_result
XA RECOVER;
XA ROLLBACK 0x4142434445, 0x46, 0x02 ;

--disable_query_log
call mtr.add_suppression("Found 1 prepared XA transactions");
--enable_query_log

--echo #
--echo # WL#7155: Test 1: check that if a thread of control terminates then
--echo # the Resource Manager dissociate and rollback any associated
--echo # transaction branch (see ref. 3.6 on page 18 of XA spec)
--echo #

--connect (con1,localhost,root,,)

CREATE TABLE t1 (a INT) ENGINE=INNODB;
XA START 'xid1';
INSERT INTO t1 VALUES (1);
XA END 'xid1';

--disconnect con1
--source include/wait_until_disconnected.inc
--connection default

SELECT * FROM t1;
DROP TABLE t1;

--echo #
--echo # WL#7155: Test 2: Check that if the Resource Manager is doing work outside
--echo # any global transaction on behalf of the application,
--echo # xa_start() returns XAER_OUTSIDE (see xa_start description on page 52 of XA spec)
--echo #

--connect (con1,localhost,root,,)

SET SESSION autocommit=0;
START TRANSACTION;
--error ER_XAER_OUTSIDE
XA START 'xid1';
COMMIT;

--disconnect con1
--source include/wait_until_disconnected.inc
--connection default

--echo #
--echo # WL#7155: Test 3: Check that the Resource Manager returns error
--echo # if the Transaction Manager tries to resume non-existent transaction.
--echo #

--error ER_XAER_INVAL
XA START 'xid1' RESUME;

--echo #
--echo # WL#7155: Test 4: Check that the Resource Manager returns ok
--echo # if the Transaction Manager tries to resume transaction
--echo # that has been ended before.
--echo #

--connect (con1,localhost,root,,)

XA START 'xid1';
XA END 'xid1';
XA START 'xid1' RESUME;
XA END 'xid1';

--disconnect con1
--source include/wait_until_disconnected.inc
--connection default

--echo #
--echo # WL#7155: Test 5: Check that the Resource Manager returns error
--echo # if the Transaction Manager ends some XA transaction and
--echo # starts another one with RESUME clause right after that.
--echo #

--connect (con1,localhost,root,,)

XA START 'xid1';
XA END 'xid1';
--error ER_XAER_NOTA
XA START 'xid2' RESUME;
--disconnect con1
--source include/wait_until_disconnected.inc
--connection default

--echo #
--echo # WL#7155: Test 6: Check that the SUSPEND clause isn't supported for XA END.
--echo #

--connect (con1,localhost,root,,)

XA START 'xid1';
--error ER_XAER_INVAL
XA END 'xid1' SUSPEND;
XA END 'xid1';

--disconnect con1
--source include/wait_until_disconnected.inc
--connection default

--echo #
--echo # WL#7155: Test 7: Check that attempt to end non-existent XA transaction
--echo # while another XA transaction is active leads to an error
--echo #

--connect (con1,localhost,root,,)

XA START 'xid1';
--error ER_XAER_NOTA
XA END 'xid2';
XA END 'xid1';

--disconnect con1
--source include/wait_until_disconnected.inc
--connection default

--echo #
--echo # WL#7155: Test 8: Check that XA ROLLBACK can't be called for active XA transaction
--echo #

--connect (con1,localhost,root,,)

XA START 'xid1';
--error ER_XAER_RMFAIL
XA ROLLBACK 'xid1';
XA END 'xid1';

--disconnect con1
--source include/wait_until_disconnected.inc
--connection default

--echo #
--echo # WL#7155: Test 9: Check that XA PREPARE returns error for unknown xid
--echo #

--connect (con1,localhost,root,,)

XA START 'xid1';
XA END 'xid1';
--error ER_XAER_NOTA
XA PREPARE 'xid2';

--disconnect con1
--source include/wait_until_disconnected.inc
--connection default

--echo #
--echo # WL#7155: Test 10: Check that rollback of XA transaction with unknown xid
--echo # leads to an error when there is other prepared XA transaction.
--echo #

XA START 'xid1';
XA END 'xid1';
XA PREPARE 'xid1';
--error ER_XAER_NOTA
XA ROLLBACK 'xid2';
XA ROLLBACK 'xid1';

--echo #
--echo # Behavior of XA transaction when autocommit = OFF
--echo #
CREATE TABLE t1(i INT);
INSERT INTO t1 VALUES (1);
SET SESSION autocommit = OFF;
XA START 'xid3';
INSERT INTO t1 VALUES (2);
XA END 'xid3';
XA PREPARE 'xid3';
--echo # XA COMMIT/ROLLBACK is ok as we are not in active multi-statement
--echo # transaction immediately after XA PRPEARE
XA ROLLBACK 'xid3';

XA START 'xid4';
INSERT INTO t1 VALUES (3);
XA END 'xid4';
XA PREPARE 'xid4';
--echo # This starts another multi-statement transaction
INSERT INTO t1 VALUES (4);
--echo # Which causes XA COMMIT/ROLLBACK to fail
--error ER_XAER_RMFAIL
XA COMMIT 'xid4';
ROLLBACK WORK;
--echo # After ending multi-statement transaction, we can again commit
--echo # XA transaction
XA COMMIT 'xid4';
SELECT * FROM t1 ORDER BY i;
SET SESSION autocommit = DEFAULT;
DROP TABLE t1;

--echo #
--echo # Bug#18107853 - XA LIST GETS CORRUPT, CRASH AND/OR HANG AND/OR ASSERTION
--echo #

--echo # Check that the server reports an error in case of too long input value of
--echo # format ID overflows the type of unsigned long

--error ER_PARSE_ERROR
XA START '1', 0x01, 18446744073709551615;

--echo #
--echo # Bug#25364178 - XA PREPARE INCONSISTENT WITH XTRABACKUP
--echo #

--echo # Check XA state when lock_wait_timeout happens
--echo # More tests added to flush_read_lock.test
--connect (con_tmp,localhost,root,,)
SET SESSION lock_wait_timeout=1;
CREATE TABLE asd (a INT);
XA START 'test1';
INSERT INTO asd VALUES (1);
XA END 'test1';

--connection default
FLUSH TABLE WITH READ LOCK;
--connection con_tmp
--echo # PREPARE error will do auto rollback.
--ERROR ER_LOCK_WAIT_TIMEOUT
XA PREPARE 'test1';
SHOW ERRORS;
--connection default
UNLOCK TABLES;

--connection con_tmp
XA START 'test1';
INSERT INTO asd VALUES (1);
XA END 'test1';
XA PREPARE 'test1';
--connection default
FLUSH TABLES WITH READ LOCK;
--connection con_tmp
--echo # LOCK error during ROLLBACK will not alter transaction state.
--error ER_LOCK_WAIT_TIMEOUT
XA ROLLBACK 'test1';
SHOW ERRORS;
--sorted_result
XA RECOVER;
--echo # LOCK error during COMMIT will not alter transaction state.
--error ER_LOCK_WAIT_TIMEOUT
XA COMMIT 'test1';
SHOW ERRORS;
--sorted_result
XA RECOVER;
--connection default
UNLOCK TABLES;
--connection con_tmp
XA ROLLBACK 'test1';
--sorted_result
XA RECOVER;
DROP TABLE asd;
--disconnect con_tmp
--source include/wait_until_disconnected.inc
--connection default

# Finish off  disconnected survived transaction
--echo There should be practically no error, but in theory
--echo XAER_NOTA: Unknown XID can be returned if con1 disconnection
--echo took for too long.
--echo todo: consider to make this test dependent on P_S if
--echo todo: such case will be ever registered.

# There should be no prepared transactions left.
XA RECOVER;


--echo #
--echo # WL#7194 -- Define and implement authorization model to manage XA-transactions
--echo #

CREATE USER u1;
GRANT XA_RECOVER_ADMIN ON *.* TO u1;

CREATE USER u2;

XA START 'xid1';
XA END 'xid1';
XA PREPARE 'xid1';

--echo # Connect as user u1
--echo # Since the privilege XA_RECOVER_ADMIN was granted to the user u1
--echo # it is allowed to execute the statement XA RECOVER to get a list of
--echo # xids for prepared XA transactions.
--connect(con1,localhost,u1,'',)
--sorted_result
XA RECOVER;

--echo # Connect as user u2
--echo # The privilege XA_RECOVER_ADMIN wasn't granted to the user u2.
--echo # It leads to issuing the error ER_XAER_RMERR on attempt to run
--echo # the statement XA RECOVER.
--connect(con2,localhost,u2,'',)
--error ER_XAER_RMERR
XA RECOVER;
SHOW WARNINGS;
--connection con1
--disconnect con1
--source include/wait_until_disconnected.inc

--connection default
--echo # The default connection was established on behalf the user root@localhost
--echo # who has the XA_RECOVER_ADMIN privilege assigned by default.
--echo # So for the user root@localhost the statement XA RECOVER
--echo # can be executed successfully.
--sorted_result
XA RECOVER;
XA COMMIT 'xid1';

--echo # Connect as the user u2 who wasn't granted privilege XA_RECOVER_ADMIN.
--echo # Initiates a new XA transaction on behalf the user u2 and shows that
--echo # call XA RECOVER made by the user u2 is failed with error ER_XAER_RMERR.

--connection con2
XA START 'xid2';
XA END 'xid2';
XA PREPARE 'xid2';
--error ER_XAER_RMERR
XA RECOVER;
SHOW WARNINGS;
--echo # Although the user u2 can't get a list of prepared XA transaction this user
--echo # can finalize a prepared XA transaction knowing its XID value.
XA COMMIT 'xid2';
--disconnect con2
--source include/wait_until_disconnected.inc

--connection default
DROP USER u1, u2;

--echo # Check that a user who has the privilege SUPER and hasn't the privilege
--echo # XA_RECOVER_ADMIN isn't allowed to run the statement XA RECOVER
CREATE USER u1;
GRANT SUPER ON *.* TO u1;
SHOW GRANTS FOR u1;

--connect(con1,localhost,u1,'',)
XA START 'xid1';
XA END 'xid1';
XA PREPARE 'xid1';

--error ER_XAER_RMERR
XA RECOVER;
XA COMMIT 'xid1';

--disconnect con1
--source include/wait_until_disconnected.inc

--connection default
DROP USER u1;

--echo # End of tests fro WL#7194

--echo #
--echo # Bug #26848877 -- XA COMMIT/ROLLBACK REJECTED BY NON-AUTOCOMMIT SESSION
--echo #                  WITH NO ACTIVE TRANSACTION
--echo #

--connect(con1,localhost,root,'',)

--echo # Check that XA COMMIT finalizes XA transaction branch in case
--echo # XA transaction was prepared successfully and after session reconnect
--echo # a user turned off autocommit before running XA COMMIT
XA START 'xid1';
XA END 'xid1';
XA PREPARE 'xid1';

--disconnect con1
--source include/wait_until_disconnected.inc

--connection default

SET autocommit = 0;
XA RECOVER;
# Without patch the following statement would fail with error message:
# 'XA COMMIT 'xid1'' failed: 1399: XAER_RMFAIL: The command can't be executed
# when global transaction is in the  NON-EXISTING state
XA COMMIT 'xid1';

--echo # Check that XA ROLLBACK finalizes XA transaction branch in case
--echo # XA transaction was prepared successfully and after reconnect
--echo # a user turned off autocommit before running XA ROLLBACK

--connect(con1,localhost,root,'',)

XA START 'xid1';
XA END 'xid1';
XA PREPARE 'xid1';

--disconnect con1
--source include/wait_until_disconnected.inc

--connection default

SET autocommit = 0;
XA RECOVER;
# Without patch the following statement would fail with error message:
# 'XA ROLLBACK 'xid1'' failed: 1399: XAER_RMFAIL: The command cannot be
# executed when global transaction is in the  NON-EXISTING state
XA ROLLBACK 'xid1';
SET autocommit = DEFAULT;


--echo #
--echo # BUG 31030205 - XA PREPARED TXN WILL STAY AS "RECOVERED TRX" IF ROLLBACK
--echo #                XID HAS WRONG FORMATID
--echo #
--echo #

--connect(con1,localhost,root,'',)
CREATE TABLE t1 (a INT) ENGINE=INNODB;
XA START X'1A2B3C4D5E6F',X'F6E5D4C3B2A1',12345;
INSERT INTO t1 VALUES (1);
XA END X'1A2B3C4D5E6F',X'F6E5D4C3B2A1',12345;
XA PREPARE X'1A2B3C4D5E6F',X'F6E5D4C3B2A1',12345;
--disconnect con1
--source include/wait_until_disconnected.inc
--connect(con2, localhost, root, '',)
--error ER_XAER_NOTA
XA ROLLBACK X'1A2B3C4D5E6F',X'F6E5D4C3B2A1',2;
XA RECOVER CONVERT XID;
XA ROLLBACK X'1A2B3C4D5E6F',X'F6E5D4C3B2A1',12345;
DROP TABLE t1;
--disconnect con2
--source include/wait_until_disconnected.inc
--connection default

--echo # Verify that temporary tables are rejected
CREATE TEMPORARY TABLE temp1(i INT);
XA START 'xa1';
--error ER_XA_TEMP_TABLE
INSERT INTO temp1 VALUES (1),(2),(3);
XA END 'xa1';
XA ROLLBACK 'xa1';

XA START 'xa2';
--error ER_XA_TEMP_TABLE
CREATE TEMPORARY TABLE temp2(i INT);
XA END 'xa2';
XA ROLLBACK 'xa2';

XA START 'xa3';
--error ER_XA_TEMP_TABLE
SELECT * FROM temp1;
XA END 'xa3';
XA ROLLBACK 'xa3';

--echo # Verify that DROP TEMPORARY TABLE is rejected in XA transaction
INSERT INTO temp1 VALUES (1),(2),(3);
XA START 'xa3_';
--error ER_XA_TEMP_TABLE
DROP TEMPORARY TABLE temp1;
XA END 'xa3_';
XA ROLLBACK 'xa3_';

DROP TEMPORARY TABLE temp1;


XA START 'xa4';
--error ER_VARIABLE_NOT_SETTABLE_IN_TRANSACTION
SET SESSION xa_detach_on_prepare=OFF;
XA END 'xa4';
XA ROLLBACK 'xa4';


CREATE TABLE t1(c1 VARCHAR(128));

--echo # Testing XA COMMIT/ROLLBACK on the same connection

--echo # Rolling back basic XA transaction
connect(xc1,localhost,root,,);
XA START 'xa1';
INSERT INTO t1 VALUES ('Inserted by xa1');
XA END 'xa1';
XA PREPARE 'xa1';
XA ROLLBACK 'xa1';
SELECT * FROM t1;

--disconnect xc1
--source include/wait_until_disconnected.inc
--connection default

--echo # Committing basic XA transaction
--connect(xc2,localhost,root,,)
XA START 'xa2';
INSERT INTO t1 VALUES ('Inserted by xa2');
XA END 'xa2';
XA PREPARE 'xa2';
XA COMMIT 'xa2';

--disconnect xc2
--source include/wait_until_disconnected.inc
--connection default

--echo # SELECT after prepare
--connect(xc5,localhost,root,,)
XA START 'xa5';
INSERT INTO t1 VALUES ('Inserted by xa5');
XA END 'xa5';
XA PREPARE 'xa5';
SELECT * FROM t1;
XA COMMIT 'xa5';

--disconnect xc5
--source include/wait_until_disconnected.inc
--connection default
SELECT * FROM t1;

--echo # DML after prepare
--connect(xc6,localhost,root,,)
XA START 'xa6';
INSERT INTO t1 VALUES ('Inserted by xa6');
XA END 'xa6';
XA PREPARE 'xa6';
INSERT INTO t1 VALUES ('Inserted after prepare');
SELECT * FROM t1;
XA COMMIT 'xa6';

--disconnect xc6
--source include/wait_until_disconnected.inc
--connection default
SELECT * FROM t1;

--echo # DDL after prepare
--connect(xc7,localhost,root,,)
XA START 'xa7';
INSERT INTO t1 VALUES ('Inserted by xa7');
XA END 'xa7';
XA PREPARE 'xa7';
CREATE TABLE t2(i INT);
INSERT INTO t2 VALUES (1), (2), (3);
ALTER TABLE t2 ADD COLUMN j INT;
INSERT INTO t2 VALUES (4,4),(5,5);
XA COMMIT 'xa7';

--disconnect xc7
--source include/wait_until_disconnected.inc
--connection default
SELECT * FROM t1;
SELECT * FROM t2;
DROP TABLE t2;

--echo # BEGIN WORK after prepare
--connect(xc8,localhost,root,,)
XA START 'xa8';
INSERT INTO t1 VALUES ('Inserted by xa8');
XA END 'xa8';
XA PREPARE 'xa8';
SET SESSION autocommit = OFF;
BEGIN WORK;
INSERT INTO t1 VALUES ('Inserted by normal transaction');
ROLLBACK WORK;
XA COMMIT 'xa8';

--disconnect xc8
--source include/wait_until_disconnected.inc
--connection default
SELECT * FROM t1;

--echo # SAVEPOINT after prepare
--connect(xc10,localhost,root,,)
SET SESSION autocommit=OFF;

XA START 'xa10';
INSERT INTO t1 VALUES ('Inserted by xa10');
XA END 'xa10';
XA PREPARE 'xa10';
SAVEPOINT s1;
INSERT INTO t1 VALUES ('Inserted in savepoint s1');
ROLLBACK TO s1;
INSERT INTO t1 VALUES ('Inserted by normal transaction');
COMMIT WORK;
XA COMMIT 'xa10';

--disconnect xc10
--source include/wait_until_disconnected.inc
--connection default
SELECT * FROM t1;

--echo # HANDLER read after prepare
--connect(xc11,localhost,root,,)
XA START 'xa11';
INSERT INTO t1 VALUES ('Inserted by xa11');
XA END 'xa11';
XA PREPARE 'xa11';
HANDLER t1 OPEN;
HANDLER t1 READ FIRST;
HANDLER t1 close;
XA COMMIT 'xa11';

--disconnect xc11
--source include/wait_until_disconnected.inc
--connection default
SELECT * FROM t1;

--echo ### Test XA COMMIT/ROLLBACK from other connection

--echo # Rolling back basic XA transaction from other connection
--connect(xc3,localhost,root,,)
XA START 'xa3';
INSERT INTO t1 VALUES ('Inserted by xa3');
XA END 'xa3';
XA PREPARE 'xa3';
SELECT object_type, object_name, lock_type, lock_duration, lock_status
FROM performance_schema.metadata_locks WHERE object_schema = 'test';

--connect(xc3r,localhost,root,,)
XA ROLLBACK 'xa3';
--connection default
SELECT * FROM t1;
SELECT object_type, object_name, lock_type, lock_duration, lock_status
FROM performance_schema.metadata_locks WHERE object_schema = 'test';

--connection xc3r
--disconnect xc3r
--source include/wait_until_disconnected.inc
--connection xc3
--disconnect xc3
--source include/wait_until_disconnected.inc
--connection default

--echo # Committing basic XA transaction from other connection
connect(xc4,localhost,root,,);
XA START 'xa4';
INSERT INTO t1 VALUES ('Inserted by xa4');
XA END 'xa4';
XA PREPARE 'xa4';
SELECT object_type, object_name, lock_type, lock_duration, lock_status
FROM performance_schema.metadata_locks WHERE object_schema = 'test';

--connect(xc4c,localhost,root,,)
XA COMMIT 'xa4';
--connection default
SELECT * FROM t1;
SELECT object_type, object_name, lock_type, lock_duration, lock_status
FROM performance_schema.metadata_locks WHERE object_schema = 'test';

--connection xc4c
--disconnect xc4c
--source include/wait_until_disconnected.inc

--connection xc4
--disconnect xc4
--source include/wait_until_disconnected.inc

--connection default
DROP TABLE t1;

--echo # Select-only transaction
CREATE TABLE t1(d VARCHAR(128));
INSERT INTO t1 VALUES ('Row 1'), ('Row 2');

--echo # connect(con3)
--connect(con3,localhost,root,,)
XA START 'xa3';
SELECT * FROM t1;
XA END 'xa3';
XA PREPARE 'xa3';
XA START 'xa3x';
SELECT d FROM t1;
XA END 'xa3x';
XA PREPARE 'xa3x';

--echo # disconnect(con3)
--disconnect con3
--source include/wait_until_disconnected.inc

--echo # connection default
--connection default

XA ROLLBACK 'xa3';
XA COMMIT 'xa3x';
DROP TABLE t1;

--echo # Check that the necessary privileges are required for modifying the sysvar
CREATE USER xau;
--connect(con4,localhost,xau,,)
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET xa_detach_on_prepare = OFF;
--disconnect con4
--source include/wait_until_disconnected.inc

--connection default
GRANT GROUP_REPLICATION_ADMIN ON *.* TO xau;

--connect(con5,localhost,xau,,)
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET xa_detach_on_prepare = OFF;
--disconnect con5
--source include/wait_until_disconnected.inc

--connection default
GRANT SESSION_VARIABLES_ADMIN ON *.* TO xau;
--connect(con6,localhost,xau,,)
SET xa_detach_on_prepare = OFF;
--disconnect con6
--source include/wait_until_disconnected.inc

--connection default
DROP USER xau;


--echo # Check that the set of XA transactions after restart

--echo # Normal modifying transaction
CREATE TABLE t7(i INT);
connect(con7,localhost,root,,);
XA START 'xa7';
INSERT INTO t7 VALUES (1),(2),(3);
XA END 'xa7';
XA PREPARE 'xa7';

--disconnect con7
--source include/wait_until_disconnected.inc

--connection default

XA RECOVER;

--let $restart_parameters = restart:
--source include/restart_mysqld.inc
XA RECOVER;
XA ROLLBACK 'xa7';
SELECT * FROM t7;
DROP TABLE t7;

--echo # Pure select transaction (optimized away in Innodb)
--echo # This optimization is non-std and may change in the future.
CREATE TABLE t8(i INT);
INSERT INTO t8 VALUES (1),(2),(3);
connect(con8,localhost,root,,);
XA START 'xa8';
SELECT * FROM t8;
XA END 'xa8';
XA PREPARE 'xa8';

--disconnect con8
--source include/wait_until_disconnected.inc

--connection default

XA RECOVER;

--let $restart_parameters = restart:
--source include/restart_mysqld.inc
XA RECOVER;
--error ER_XAER_NOTA
XA ROLLBACK 'xa8';
DROP TABLE t8;

--disable_connect_log
