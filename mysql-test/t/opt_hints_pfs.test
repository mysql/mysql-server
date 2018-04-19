# Test requires: sp-protocol/ps-protocol/view-protocol/cursor-protocol disabled
--source include/no_protocol.inc

--echo #
--echo # Bug #21286261: QUERY DIGEST DOES NOT REFLECT NEW OPTIMIZER HINTS
--echo #

CREATE TABLE t1(i INT);
CREATE TABLE t2(i INT);

let $check=SELECT digest, digest_text FROM performance_schema.events_statements_history ORDER BY timer_start DESC LIMIT 2;

--echo # Digests should be same (empty hint comment):

--disable_result_log
SELECT        * FROM t1;
SELECT /*+ */ * FROM t1;
--enable_result_log
eval $check;

--echo # Digests should be different:

--disable_result_log
SELECT * FROM t1, t2;
SELECT /*+
  BKA(t1@qb1)
  BNL(@qb1 t1)
  DUPSWEEDOUT
  FIRSTMATCH
  INTOEXISTS
  LOOSESCAN
  MATERIALIZATION
  MRR(t1)
  NO_BKA(t2)
  NO_BNL(t2)
  NO_ICP(t2)
  NO_MRR(t2)
  NO_RANGE_OPTIMIZATION(t2)
  NO_SEMIJOIN(t2)
  QB_NAME(qb1)
  SEMIJOIN(t1)
  SUBQUERY(t1)
*/ * FROM t1, t2;
--enable_result_log
eval $check;

--disable_result_log
SELECT * FROM t2, t1;
SELECT /*+ MAX_EXECUTION_TIME(4294967295) */ * FROM t2, t1;
--enable_result_log
eval $check;

--disable_result_log
SELECT 1;
SELECT /*+ bad_hint_also_goes_to_digest */ 1;
--enable_result_log
eval $check;

DROP TABLE t1, t2;

--echo #
--echo # WL#681: Hint to temporarily set session variable for current statement
--echo #

--disable_result_log
SELECT 1;
SELECT /*+ SET_VAR(foo = 1K) */ 1;
--enable_result_log
eval $check;

--disable_result_log
SELECT 1;
SELECT /*+ SET_VAR(bar = 'baz') */ 1;
--enable_result_log
eval $check;

