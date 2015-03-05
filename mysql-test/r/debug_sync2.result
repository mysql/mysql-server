#
# Bug#13688248 CRASH IN DIAGNOSTICS_AREA::SET_OK_STATUS WHEN USING DEBUG_SYNC
#
SET SESSION DEBUG_SYNC= 'RESET';
CREATE TABLE t1 (pk INT, PRIMARY KEY(pk));
SET SESSION sql_mode=TRADITIONAL;
SET SESSION autocommit=1;
INSERT INTO t1 VALUES(1);
SET SESSION debug_sync='write_row_replace SIGNAL go_ahead1 WAIT_FOR comes_never ';
REPLACE INTO t1 ( pk ) VALUES ( 1 );
Warnings:
Warning	1639	debug sync point wait timed out
DROP TABLE t1;
#
# Bug#19670163 : ASSERT AFTER MISSING ERROR CHECK IN SUBQUERY
#
CREATE TABLE t1(a INT);
CREATE TABLE t2(a INT);
INSERT INTO t1 VALUES (0);
INSERT INTO t2 VALUES (0);
START TRANSACTION;
DELETE FROM t1;
connection default;
SET DEBUG_SYNC=
'lock_wait_suspend_thread_enter SIGNAL blocked WAIT_FOR delete';
# Sending:
SET @b= (SELECT(SELECT a FROM t1 FOR UPDATE) FROM t2 FOR UPDATE);
connection con1;
SET DEBUG_SYNC= 'now WAIT_FOR blocked';
SET DEBUG_SYNC= 'lock_wait_suspend_thread_enter SIGNAL delete';
DELETE FROM t2;
COMMIT;
connection default;
#Reaping "SET @b ..."
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
connection con1;
disconnect con1;
connection default;
DROP TABLE t1,t2;
SET DEBUG_SYNC= 'RESET';
