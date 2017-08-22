
--echo # Bug #22173903  XA+GTID: ASSERT 'THD->OWNED_GTID.IS_EMPTY()'
--echo # AT RPL_GTID_STATE.CC:614
--echo
--enable_connect_log

--echo # Test 1: Commit Test
connect (con1,localhost,root);
CREATE TABLE t1 (a INT);
XA START 'xa1';
INSERT INTO t1 VALUES (1);
XA END 'xa1';
XA PREPARE 'xa1';
disconnect con1;
--source include/wait_until_disconnected.inc

connection default;
BEGIN;
INSERT INTO t1 VALUES(1);
--error ER_XAER_RMFAIL
XA COMMIT 'xa1';
COMMIT;
XA COMMIT 'xa1';
DROP TABLE t1;

--echo
--echo # Test 2 : Rollback Test
connect (con1,localhost,root);
CREATE TABLE t1 (a INT);
XA START 'xa1';
INSERT INTO t1 VALUES (1);
XA END 'xa1';
XA PREPARE 'xa1';
disconnect con1;
--source include/wait_until_disconnected.inc

connection default;
BEGIN;
INSERT INTO t1 VALUES(1);
--error ER_XAER_RMFAIL
XA ROLLBACK 'xa1';
COMMIT;
XA ROLLBACK 'xa1';
DROP TABLE t1;

--disable_connect_log
