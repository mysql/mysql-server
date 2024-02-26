--source include/have_debug.inc

#
# Bug #37627: Killing query with sum(exists()) or avg(exists()) reproducibly
# crashes server
#

CREATE TABLE t1(id INT);
INSERT INTO t1 VALUES (1),(2),(3),(4);
INSERT INTO t1 SELECT a.id FROM t1 a,t1 b,t1 c,t1 d;
# Setup the mysqld to crash at certain point
SET @orig_debug = @@debug;
SET SESSION debug="d,subselect_exec_fail";
SELECT SUM(EXISTS(SELECT RAND() FROM t1)) FROM t1;
SELECT REVERSE(EXISTS(SELECT RAND() FROM t1));
SET SESSION debug=@orig_debug;
DROP TABLE t1;

--echo #
--echo # Bug#21383882 ASSERTION FAILED: 0 IN SELECT_LEX::PRINT()
--echo #

CREATE TABLE t1(a INT);
INSERT INTO t1 VALUES(1),(1);
--error ER_SUBQUERY_NO_1_ROW
SELECT ((SELECT 1 FROM t1) IN (SELECT 1 FROM t1)) - (11111111111111111111);
DROP TABLE t1;

--echo #
--echo # Bug#26679495: SIG 11 IN SUBSELECT_HASH_SJ_ENGINE::CLEANUP
--echo #

CREATE TABLE t (x INT);
INSERT INTO t VALUES (1), (2), (3);
ANALYZE TABLE t;
--let $query= SELECT * FROM t WHERE x IN (SELECT COUNT(*) FROM t GROUP BY x)
--echo # The subquery should be materialized so that we
--echo # use subselect_hash_sj_engine.
--eval EXPLAIN $query
--eval $query
--echo # Execute the query with a simulated error in
--echo # subselect_hash_sj_engine::setup().
SET DEBUG='+d,hash_semijoin_fail_in_setup';
--skip_if_hypergraph   # Uses hash semijoin instead, so doesn't get the error.
--error ER_UNKNOWN_ERROR
--eval $query
SET DEBUG='-d,hash_semijoin_fail_in_setup';
DROP TABLE t;

--echo #
--echo # Bug#26679983: SIG 11 IN MAKE_JOIN_READINFO|SQL/SQL_SELECT.CC
--echo #

CREATE TABLE t (x INT);
INSERT INTO t VALUES (1), (2), (3);
ANALYZE TABLE t;
--let $query= SELECT * FROM t WHERE x IN (SELECT x FROM t)
--echo # We want nested loop with duplicate weedout to reproduce the bug.
SET optimizer_switch = 'firstmatch=off,materialization=off';
--eval EXPLAIN $query
--sorted_result
--eval $query
--echo # Execute the query with a simulated error in
--echo # create_duplicate_weedout_tmp_table().
SET DEBUG='+d,create_duplicate_weedout_tmp_table_error';
--skip_if_hypergraph   # Uses hash semijoin instead (does not support weedout), so doesn't get the error.
--error ER_UNKNOWN_ERROR
--eval $query
SET DEBUG='-d,create_duplicate_weedout_tmp_table_error';
DROP TABLE t;
SET optimizer_switch = DEFAULT;

--echo #
--echo # Bug#31117893 - GROUP BY WILL THROW TABLE IS
--echo #   FULL WHEN TEMPTABLE MEMORY ALLOCATION EXCEED LIMIT
--echo #

CREATE TABLE t1(
  id INT,
  pad VARCHAR(60),
  pad1 VARCHAR(513)
);

INSERT INTO t1 VALUES (1, REPEAT('a',59), REPEAT('a',512));
INSERT INTO t1 VALUES (2, REPEAT('a',59), REPEAT('a',512));

SET SESSION debug = '+d, simulate_temp_storage_engine_full';
SELECT COUNT(*), pad FROM t1 GROUP BY pad;
# Group-by a bigger column forces the temp-table to use a
# hash based unique constraint.
SELECT COUNT(*), pad1 FROM t1 GROUP BY pad1;
SET SESSION debug = '-d, simulate_temp_storage_engine_full';

DROP TABLE t1;
