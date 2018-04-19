# Bug #22173903  XA+GTID: ASSERT 'THD->OWNED_GTID.IS_EMPTY()'
# AT RPL_GTID_STATE.CC:614

# Test 1: Commit Test
connect  con1,localhost,root;
CREATE TABLE t1 (a INT);
XA START 'xa1';
INSERT INTO t1 VALUES (1);
XA END 'xa1';
XA PREPARE 'xa1';
disconnect con1;
connection default;
BEGIN;
INSERT INTO t1 VALUES(1);
XA COMMIT 'xa1';
ERROR XAE07: XAER_RMFAIL: The command cannot be executed when global transaction is in the  NON-EXISTING state
COMMIT;
XA COMMIT 'xa1';
DROP TABLE t1;

# Test 2 : Rollback Test
connect  con1,localhost,root;
CREATE TABLE t1 (a INT);
XA START 'xa1';
INSERT INTO t1 VALUES (1);
XA END 'xa1';
XA PREPARE 'xa1';
disconnect con1;
connection default;
BEGIN;
INSERT INTO t1 VALUES(1);
XA ROLLBACK 'xa1';
ERROR XAE07: XAER_RMFAIL: The command cannot be executed when global transaction is in the  NON-EXISTING state
COMMIT;
XA ROLLBACK 'xa1';
DROP TABLE t1;
