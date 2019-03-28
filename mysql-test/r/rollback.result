#
# Bug #27729974 "ASSERTION `!M_THD->TRANSACTION_ROLLBACK_REQUEST' AT
#                THD::ATTACHABLE_TRX::INIT".
#
CREATE TABLE t1 (i INT);
CREATE TABLE t0 (j INT);
BEGIN;
CREATE TEMPORARY TABLE tt AS SELECT * FROM t1;
INSERT INTO tt VALUES (1), (2), (3);
DROP TEMPORARY TABLE tt;
connect  con1, localhost, root,,;
DROP TABLES t0, t1;
connection default;
# Wait until DROP TABLES is blocked due to transaction
# owning metadata lock on t1.
# The below statement causes MDL deadlock which triggers transaction
# rollback. Prior to fix assertion has failed during rollback.
SELECT * FROM t0;
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
connection con1;
disconnect con1;
connection default;
