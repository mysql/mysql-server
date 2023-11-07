--source include/have_hypergraph.inc
--source include/elide_costs.inc

--echo #
--echo # Bug#33616174: Querying data dictionary table KEY_COLUMN_USAGE is
--echo #               slow with hypergraph enabled
--echo #

CREATE TABLE t1(x INT);
CREATE TABLE t2(pk INT PRIMARY KEY);
INSERT INTO t1 VALUES (1);
INSERT INTO t2 VALUES (1), (2), (3), (4), (5), (6), (7), (8), (9), (10);
ANALYZE TABLE t1, t2;

# Used to choose a table scan on t2. Should pick an index lookup.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE
SELECT 1 FROM t1, LATERAL (SELECT DISTINCT t1.x) AS dt, t2
WHERE t2.pk = dt.x;

# Used to fail because the hypergraph optimizer could not find a plan.
SELECT 1 FROM
  t1,
  LATERAL (SELECT DISTINCT t1.x) AS dt1,
  LATERAL (SELECT DISTINCT dt1.x) AS dt2
WHERE dt1.x = dt2.x;

DROP TABLE t1, t2;

--source include/disable_hypergraph.inc
