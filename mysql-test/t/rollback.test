--echo #
--echo # Bug #27729974 "ASSERTION `!M_THD->TRANSACTION_ROLLBACK_REQUEST' AT
--echo #                THD::ATTACHABLE_TRX::INIT".
--echo #
--enable_connect_log
CREATE TABLE t1 (i INT);
CREATE TABLE t0 (j INT);
BEGIN;
CREATE TEMPORARY TABLE tt AS SELECT * FROM t1;
INSERT INTO tt VALUES (1), (2), (3);
DROP TEMPORARY TABLE tt;

--connect (con1, localhost, root,,)
--send DROP TABLES t0, t1

connection default;
--echo # Wait until DROP TABLES is blocked due to transaction
--echo # owning metadata lock on t1.
let $wait_condition=
  SELECT COUNT(*) = 1 FROM information_schema.processlist
  WHERE state = "Waiting for table metadata lock" AND
        info LIKE "DROP TABLES t0, t1";
--source include/wait_condition.inc
--echo # The below statement causes MDL deadlock which triggers transaction
--echo # rollback. Prior to fix assertion has failed during rollback.
--error ER_LOCK_DEADLOCK
SELECT * FROM t0;

--connection con1
--reap
--disconnect con1
--source include/wait_until_disconnected.inc

--connection default

--disable_connect_log
