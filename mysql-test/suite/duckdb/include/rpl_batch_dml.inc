# Exercise inserts, primary/non-primary-key updates, deletes and mixed DML.
# Requires rpl_batch_create_wide_table.inc.
# Set $duckdb_batch_multi_trx to 0 to synchronize after each phase, or to 1
# to rotate the source binlog after UPDATE and synchronize only at the end.

--echo ###########################################################################
--echo # 1. Test Insert
--echo ###########################################################################
--source include/rpl/connection_source.inc
INSERT INTO t1 (id, c0, c1, c2, c3, c4, c5) VALUES
(1, 'abc', NULL, 1.23, '2020-11-20', NULL, X'e18080ff8a'),
(2, 'd', NULL, NULL, '1998-01-01', '2006-02-28 00:21:11', NULL),
(3, 'efg', 'RSA', 5.2, NULL, '2028-09-09 15:30:34', NULL),
(4, 'hijk', NULL, 6, '2023-06-07', NULL, X'1a53aa890ee6'),
(5, 'lmn', 'RSA', 10.5, NULL, '1988-10-01 08:58:25', NULL);

if (!$duckdb_batch_multi_trx)
{
  --source include/rpl/sync.inc
  --let $rpl_diff_statement = SELECT id, c0, c1, c2, c3, c4, HEX(c5) FROM test.t1 ORDER BY id;
  --source include/rpl/diff.inc
}

--echo ###########################################################################
--echo # 2. Test Update
--echo ###########################################################################
--source include/rpl/connection_source.inc
BEGIN;
--echo # Test modify non-PK field
UPDATE t1 SET c1='xxxxx', c2=7.79, c3=NULL, c4='2025-03-25 20:58:23', c5=X'e18080e18080' WHERE id=4;
UPDATE t1 SET c2=8.88 WHERE id=5;
--echo # Test modify PK field
UPDATE t1 SET c0='yyyyyy' WHERE id=3;
UPDATE t1 SET id=6, c4='2026-03-27 10:00:34' WHERE id=1;
COMMIT;

if ($duckdb_batch_multi_trx)
{
  FLUSH LOGS;
}

if (!$duckdb_batch_multi_trx)
{
  --source include/rpl/sync.inc
  --let $rpl_diff_statement = SELECT id, c0, c1, c2, c3, c4, HEX(c5) FROM test.t1 ORDER BY id;
  --source include/rpl/diff.inc
}

--echo ###########################################################################
--echo # 3. Test Delete
--echo ###########################################################################
--source include/rpl/connection_source.inc
BEGIN;
DELETE FROM t1 WHERE id=2;
DELETE FROM t1 WHERE id=5;
COMMIT;

if (!$duckdb_batch_multi_trx)
{
  --source include/rpl/sync.inc
  --let $rpl_diff_statement = SELECT id, c0, c1, c2, c3, c4, HEX(c5) FROM test.t1 ORDER BY id;
  --source include/rpl/diff.inc
}

--echo ###########################################################################
--echo # 4. Test Mix Transaction
--echo ###########################################################################
--source include/rpl/connection_source.inc
BEGIN;
DELETE FROM t1 WHERE id=3;
UPDATE t1 SET c1='zzzzzz' WHERE id=1;
INSERT INTO t1 (id, c0, c1, c2, c3, c4, c5) VALUES (3, 'rds', 'RSA', 5.7, NULL, '2028-01-01 10:11:10', NULL);
INSERT INTO t1 (id, c0) VALUES (2, 'hhhhh');
UPDATE t1 SET c0='mysql' WHERE id=4;
DELETE FROM t1 WHERE id=2;
COMMIT;

--source include/rpl/sync.inc
--let $rpl_diff_statement = SELECT id, c0, c1, c2, c3, c4, HEX(c5) FROM test.t1 ORDER BY id;
--source include/rpl/diff.inc

# Do not leave this helper option set for later includes.
--let $duckdb_batch_multi_trx= 0
