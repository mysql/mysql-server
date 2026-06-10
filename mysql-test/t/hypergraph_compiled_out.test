# WL#17265: Hypergraph optimizer should be rejected when compiled out

--source include/not_built_with_hypergraph.inc

--echo # SET optimizer_switch fails with ER_HYPERGRAPH_NOT_SUPPORTED_YET
--echo # Error text should mention WITH_HYPERGRAPH_OPTIMIZER=OFF
--error ER_HYPERGRAPH_NOT_SUPPORTED_YET
SET SESSION optimizer_switch='hypergraph_optimizer=on';

--error ER_HYPERGRAPH_NOT_SUPPORTED_YET
SET GLOBAL optimizer_switch='hypergraph_optimizer=on';

--error ER_HYPERGRAPH_NOT_SUPPORTED_YET
SET PERSIST optimizer_switch='hypergraph_optimizer=on';

--echo # SET PERSIST fails on repeated attempts
--error ER_HYPERGRAPH_NOT_SUPPORTED_YET
SET PERSIST optimizer_switch='hypergraph_optimizer=on';

--echo # SET_VAR should execute but produce warning
CREATE TABLE t1 (a INT PRIMARY KEY, b INT);
CREATE TABLE t2 (a INT PRIMARY KEY, c INT);
INSERT INTO t1 VALUES (1,10),(2,20),(3,30);
INSERT INTO t2 VALUES (1,100),(2,200),(4,400);

--disable_result_log
SELECT /*+ SET_VAR(optimizer_switch='hypergraph_optimizer=on') */ t1.a
  FROM t1 JOIN t2 ON t1.a=t2.a
  WHERE t1.b >= 20;
--enable_result_log

SHOW WARNINGS;

--echo # Hypergraph optimizer should not be used when compiled out
SET SESSION optimizer_trace='enabled=on';
SET SESSION optimizer_trace_max_mem_size=1048576;

--disable_result_log
SELECT /*+ SET_VAR(optimizer_switch='hypergraph_optimizer=on') */ t1.a
  FROM t1 JOIN t2 ON t1.a=t2.a
  WHERE t1.b >= 20;
--enable_result_log

--echo # Expect COUNT(*) = 0 (no 'Constructed hypergraph' marker)
SELECT COUNT(*)
  FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE
  WHERE TRACE LIKE '%Constructed hypergraph%';

DROP TABLE t2;
DROP TABLE t1;
