--source include/have_debug.inc
# Costs and therefore plan choices depend on the block size.
--source include/have_innodb_16k.inc
--source include/have_hypergraph.inc
--source include/elide_costs.inc

# Tests for WL#16109 "Hypergraph cost model for large data (including IO cost)".

# Check that the ration between the cost of plan1 and plan2 is as expected.
# Return 1 if:
# ratio * (1 - tolerance) < cost(plan1)/cost(plan2) < ratio * (1 + tolerance)
CREATE FUNCTION check_cost_ratio(plan1 JSON, plan2 JSON, ratio DOUBLE,
  tolerance DOUBLE)
  RETURNS BOOLEAN DETERMINISTIC
  RETURN ABS(JSON_EXTRACT(plan1, "$.query_plan.estimated_total_cost")
         / JSON_EXTRACT(plan2, "$.query_plan.estimated_total_cost") - ratio)
         < ratio * tolerance;

SET @old_cte_max_recursion_depth=@@cte_max_recursion_depth;
SET SESSION cte_max_recursion_depth=1100;

CREATE VIEW num_1e3 AS
WITH RECURSIVE qn(n) AS (SELECT 0 UNION ALL SELECT n+1 FROM qn WHERE n<1e3)
SELECT n FROM qn;

CREATE TABLE t1(
  pk INT NOT NULL PRIMARY KEY,
  a INT NOT NULL,
  b INT NOT NULL,
  fill VARCHAR(500) NOT NULL,
  KEY k1(a)
) ENGINE=InnoDB, CHARSET=latin1;

INSERT INTO t1
SELECT n, FLOOR(RAND(0)*1e3), FLOOR(RAND(1)*1e3), REPEAT('A',500) FROM num_1e3;

ANALYZE TABLE t1;

# Check that the cost in @explain0 (no caching) is about twice the cost in
# @explain50 (half the table/index is cached).
let is_proportional=
SELECT check_cost_ratio(@explain50, @explain0, 0.5, 0.05);

# Table scan.
--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT /*+ NO_INDEX(t1) */ * FROM t1;

SET DEBUG="d,in_memory_0";
EXPLAIN FORMAT=JSON INTO @explain0 SELECT /*+ NO_INDEX(t1) */ * FROM t1;

SET DEBUG="d,in_memory_50";
EXPLAIN FORMAT=JSON INTO @explain50 SELECT /*+ NO_INDEX(t1) */ * FROM t1;

eval $is_proportional;

# These queries should be about twice as expensive if we change the cache
# hit ratio from 50% to 0%.

# Covering index lookup.
--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT pk FROM t1 WHERE a=0;

SET DEBUG="d,in_memory_0";
EXPLAIN FORMAT=JSON INTO @explain0 SELECT pk FROM t1 WHERE a=0;

SET DEBUG="d,in_memory_50";
EXPLAIN FORMAT=JSON INTO @explain50 SELECT pk FROM t1 WHERE a=0;

eval $is_proportional;

# Non-covering index range scan.
--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT * FROM t1 WHERE a<10;

SET DEBUG="d,in_memory_0";
EXPLAIN FORMAT=JSON INTO @explain0 SELECT * FROM t1 WHERE a<10;

SET DEBUG="d,in_memory_50";
EXPLAIN FORMAT=JSON INTO @explain50 SELECT * FROM t1 WHERE a<10;

eval $is_proportional;

# Covering index range scan.
--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT pk FROM t1 WHERE a<10;

SET DEBUG="d,in_memory_0";
EXPLAIN FORMAT=JSON INTO @explain0 SELECT pk FROM t1 WHERE a<10;

SET DEBUG="d,in_memory_50";
EXPLAIN FORMAT=JSON INTO @explain50 SELECT pk FROM t1 WHERE a<10;

eval $is_proportional;

SET DEBUG="d,in_memory_0";

# Reading two rows in a clustered index scan costs about the same as one,
# sice we anyway read one leaf block.
EXPLAIN FORMAT=JSON INTO @explain_1_row SELECT * FROM t1 WHERE a<1;
EXPLAIN FORMAT=JSON INTO @explain_2_rows SELECT * FROM t1 WHERE a<2;
SELECT check_cost_ratio(@explain_1_row, @explain_2_rows, 1.0, 0.01);

# Use table scan/index scan on primary if the table is not cached.
--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT b FROM t1 WHERE a<100;

# Use non-covering index scan if the table is fully cached.
SET DEBUG="d,in_memory_100";
--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT b FROM t1 WHERE a<100;

SET DEBUG="d,in_memory_0";
# Non-covering index range scan is much more expensive than covering index
# range scan, since we fetch more blocks from secondary storage.
let covering=SELECT pk FROM t1 WHERE a<10;
let non_covering=SELECT b FROM t1 WHERE a<10;
--replace_regex $elide_costs_and_rows
eval EXPLAIN FORMAT=TREE $covering;

--replace_regex $elide_costs_and_rows
eval EXPLAIN FORMAT=TREE $non_covering;

eval EXPLAIN FORMAT=JSON INTO @explain_covering $covering;
eval EXPLAIN FORMAT=JSON INTO @explain_non_covering $non_covering;
SELECT check_cost_ratio(@explain_non_covering, @explain_covering, 13.0, 0.05);

DROP TABLE t1;


CREATE TABLE t2(
  pk INT NOT NULL PRIMARY KEY,
  t1 LONGTEXT
);


INSERT INTO t2
SELECT n, REPEAT('A',12*1024) FROM num_1e3 WHERE n<10;

ANALYZE TABLE t2;

# Comprare the costs of table scans with and without a blob field that will
# be stored on overflow pages.
EXPLAIN FORMAT=JSON INTO @no_lob SELECT pk FROM t2;
EXPLAIN FORMAT=JSON INTO @lob SELECT * FROM t2;
SELECT check_cost_ratio(@lob, @no_lob, 16.0, 0.05);

DROP TABLE t2;

DROP FUNCTION  check_cost_ratio;

SET SESSION cte_max_recursion_depth= @old_cte_max_recursion_depth;
DROP VIEW num_1e3;

--source include/disable_hypergraph.inc
