# Test for optimizer tracing of subqueries

--source include/have_optimizer_trace.inc

SET optimizer_trace_max_mem_size=1048576; # 1MB
SET optimizer_trace="enabled=on,end_marker=on,one_line=off";

CREATE TABLE t1 (a INT);
CREATE TABLE t2 (a INT, b INT);
INSERT INTO t1 VALUES (2);
INSERT INTO t2 VALUES (1,7),(2,7);

--echo # Subselect execute is traced every time it is executed
SET @@optimizer_trace_features="greedy_search=off,repeated_subselect=on";
SELECT (SELECT a FROM t1 WHERE t1.a=t2.a), a FROM t2;
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;
--echo

--echo # Subselect execute is traced only the first time it is executed
SET @@optimizer_trace_features="greedy_search=off,repeated_subselect=off";
SELECT (SELECT a FROM t1 WHERE t1.a=t2.a), a FROM t2;
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;
--echo

DROP TABLE t1,t2;
SET @@optimizer_trace_features="default";


CREATE TABLE t1 (a FLOAT(5,4) zerofill);
CREATE TABLE t2 (a FLOAT(5,4),b FLOAT(2,0));

# evaluate_subselect_cond_steps for build_equal_item()
SELECT t1.a
FROM t1
WHERE t1.a= (SELECT b FROM t2 LIMIT 1) AND NOT
      t1.a= (SELECT a FROM t2 LIMIT 1) ;
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;
--echo

# evaluate_subselect_cond_steps for remove_eq_conds
SELECT 1 FROM DUAL
WHERE NOT EXISTS
  (SELECT * FROM t2 WHERE a = 50 AND b = 3);
--echo
SELECT * FROM information_schema.OPTIMIZER_TRACE;
--echo

DROP TABLE t1,t2;
