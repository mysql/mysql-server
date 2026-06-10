--source include/have_hypergraph.inc
--source include/have_optimizer_trace.inc
--source include/elide_costs.inc

# This should have been a unit test. But unit tests do not have framework
# for prepared statements yet. So we are adding this.

--echo #
--echo # Bug#34402003: HYPERGRAPH BUG: Offload issue with execute statement.
--echo #

CREATE TABLE t1(a INT);
CREATE TABLE t2(a INT);
CREATE TABLE t3(a INT);
INSERT INTO t1 VALUES (1),(2),(5);
INSERT INTO t2 VALUES (2);
INSERT INTO t3 VALUES (3);
ANALYZE TABLE t1, t2, t3;
# Hypergraph should be able to use the multiple equality (5, t1.a, t2.a).
# So, the join condition (t1.a=t2.a) should not be seen in the final plan.
# Instead, it should see filters (t1.a=5) and (t2.a=5).
SET optimizer_trace='enabled=on';
let $query = SELECT * FROM t1 LEFT JOIN t2 ON t1.a=t2.a JOIN t3 ON t1.a=5;
eval PREPARE stmt FROM "EXPLAIN FORMAT=tree $query";
--replace_regex $elide_costs
eval EXECUTE stmt;
# Check that we are using the optimized join condition for generating the
# plan i.e it should be using multiple equalities that are established during
# optimization.
SELECT
IF(TRACE LIKE '%Left join [companion set %] (extra join condition = (t1.a = 5) AND (t2.a = 5))%',
   'OK', TRACE)
FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
eval PREPARE stmt FROM "$query";
eval EXECUTE stmt;
SET optimizer_trace="enabled=off";
DROP TABLE t1,t2,t3;

CREATE TABLE t0 (a0 INTEGER);
CREATE TABLE t1 (a1 INTEGER);
CREATE TABLE t2 (a2 INTEGER);
CREATE TABLE t3 (a3 INTEGER);
INSERT INTO t0 VALUES (0),(1);
INSERT INTO t1 VALUES (0),(1);
INSERT INTO t2 VALUES (1);
INSERT INTO t3 VALUES (1);
ANALYZE TABLE t0, t1, t2, t3;
# Hypergraph should be able to detect that a1=5 cannot be true resulting
# in Zero rows access path for right side of the join.
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t0, t1 LEFT JOIN (t2,t3) ON a1=5 WHERE a0=a1 AND a0=1;
SELECT * FROM t0, t1 LEFT JOIN (t2,t3) ON a1=5 WHERE a0=a1 AND a0=1;
DROP TABLE t0,t1,t2,t3;
--echo #
--echo # Bug#34401789: Enable constant propagation in conditions
--echo #               for hypergraph optimizer
--echo #

CREATE TABLE t1 (f1 INTEGER);
ANALYZE TABLE t1;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree
SELECT f1 FROM t1 GROUP BY f1 HAVING f1 = 10 AND f1 <> 11;
DROP TABLE t1;

# This should have been a unit test. But unit tests do not have framework
# for type "year" yet.
# We are basically testing that "f1" in the non-equality predicate gets
# substituted with value "1" propagated from "f1 = 1" predicate which
# will make the condition to be always true.

--echo #
--echo # Bug#34080394: Hypergraph Offload issue : Problem in
--echo #               ExtractRequiredItemsForFilter.
--echo #

CREATE TABLE t1 (f1 YEAR);
ANALYZE TABLE t1;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t1 WHERE f1 = 1 AND f1 <> 11;
DROP TABLE t1;

--echo #
--echo # Bug#34504697: Hypergraph: Assertion
--echo #               `!(used_tabs & (~read_tables & ~filter_for_table))'
--echo #               failed
--echo #

CREATE TABLE t1 (f1 INTEGER);
SELECT 1
FROM t1 LEFT JOIN (SELECT t2.*
                   FROM (t1 AS t2 INNER JOIN t1 AS t3 ON (t3.f1 = t2.f1))
                   WHERE (t3.f1 <> 1 OR t2.f1 > t2.f1)) AS dt
ON (t1.f1 = dt.f1);
DROP TABLE t1;

--echo #
--echo # Bug#34503695:Hypergraph: mysqld crash-signal 11
--echo #              -CommonSubexpressionElimination
--echo #

CREATE TABLE t1 (f1 INTEGER);
# For the NOT IN subquery, hypergraph does re-planning with materialization.
# This replanning uses the modified where condition from the previous planning.
# For this case, the where condition is concluded as always false resulting in
# removal of elements from the OR condition leading to a crash during re-planning.
# The modified where condition from the first planning should not affect AND/OR
# structure of the condition.
SELECT * FROM t1
WHERE t1.f1 NOT IN (SELECT t2.f1
                    FROM (t1 AS t2 JOIN t1 AS t3 ON (t3.f1 = t2.f1))
                    WHERE (t3.f1 <> t2.f1 OR t3.f1 < t2.f1));
DROP TABLE t1;

--echo #
--echo # Bug#34527126: Some rapid tests in MTR fail with hypergraph
--echo #               when run in --ps-protocol mode
--echo #

CREATE TABLE t1(f1 INTEGER);
# The error generated during planning for the first derived query block
# should not result in an assert failure when the second derived table is
# cleaned up.
PREPARE ps FROM
"SELECT * FROM (WITH RECURSIVE qn AS (SELECT 1 FROM t1 UNION ALL
                                      SELECT 1 FROM t1 STRAIGHT_JOIN qn)
                                     SELECT * FROM qn) AS dt1,
                                     (SELECT COUNT(*) FROM t1) AS dt2";
--error ER_CTE_RECURSIVE_FORBIDDEN_JOIN_ORDER
EXECUTE ps;
DROP TABLE t1;

--echo #
--echo # Bug#34494877: WL#14449: Offload issue: RapidException (3):
--echo #               rpdrqctr_transcode.c:1447 @ rpdoqc_
--echo #

CREATE TABLE t(x INT, y INT);
INSERT INTO t VALUES (1, 10), (2, 20), (3, 30);
ANALYZE TABLE t;

# Expect the entire query to be optimized away. It used to produce a
# join between t and a temporary table containing the result of a
# "Zero rows" plan.
let $query =
SELECT * FROM
  t RIGHT JOIN
  (SELECT MAX(y) AS m FROM t WHERE FALSE GROUP BY x) AS dt
  ON t.x = dt.m;
--eval EXPLAIN FORMAT=TREE $query
--eval $query

# Similar to the above, but the query cannot be entirely optimized
# away, since the outer table isn't empty. It used to add a
# materialization step on top of the zero rows plan for the derived
# table. Now it should just have zero rows directly on the inner side
# of the join.
let $query =
SELECT * FROM
  t LEFT JOIN
  (SELECT MAX(y) AS m FROM t WHERE FALSE GROUP BY x) AS dt
  ON t.x = dt.m;
--replace_regex $elide_costs
--eval EXPLAIN FORMAT=TREE $query
--eval $query

# Similar case, where the query cannot be entirely optimized away.
# Verify that the entire inner side of the outer join is optimized
# away. Only t1 should be accessed.
let $query =
SELECT * FROM
  t AS t1 LEFT JOIN
  (t AS t2
   INNER JOIN (SELECT MAX(y) AS m FROM t WHERE FALSE GROUP BY x) AS dt
   ON t2.x = dt.m)
  ON t1.x = t2.y;
--replace_regex $elide_costs
--eval EXPLAIN FORMAT=TREE $query
--eval $query

DROP TABLE t;

--echo #
--echo # Bug#34534373: Heatwave offload issue - Sees inner tables of
--echo #               a semijoin when it should not
--echo #

CREATE TABLE t1 (f1 INTEGER);
ANALYZE TABLE t1;
# The condition t2.f1 = t3.f1+1 should be placed as a join condition
# for the semijoin and not on the outer join.
--replace_regex $elide_costs
EXPLAIN FORMAT=tree
 SELECT 1
 FROM t1 LEFT JOIN (SELECT * FROM t1 AS t2
                    WHERE f1 IN (SELECT f1+1 FROM t1 AS t3)) AS dt
 ON t1.f1=dt.f1;
DROP TABLE t1;

--echo #
--echo # Bug#34699398: Row estimates for joins ignores histograms.
--echo #

CREATE TABLE num (n INT);
INSERT INTO num VALUES (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);

CREATE TABLE t1 (a INT, ah INT, ai INT, KEY ix1(ai));

INSERT INTO t1 SELECT k%25, k%25, K%25 FROM
  (SELECT num1.n+num2.n*10 k FROM num num1, num num2) d1;

CREATE TABLE t2 (b INT, bh INT, bi INT, KEY ix2(bi));

INSERT INTO t2 SELECT k%25, k%25, k%25 FROM
  (SELECT num1.n+num2.n*10 k FROM num num1, num num2, num num3) d1;

ANALYZE TABLE t1 UPDATE HISTOGRAM ON ah;
ANALYZE TABLE t2 UPDATE HISTOGRAM ON bh;
ANALYZE TABLE t1,t2;

# Neither index nor histogram, so use 10% selectivity estimate.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1,t2 WHERE a=b;

--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1 x1, t1 x2 WHERE x1.a=x2.a;

--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t2 x1, t2 x2 WHERE x1.b=x2.b;

# Estimate selectivity from ix1.

--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1,t2 WHERE ai=b;

--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1 x1, t1 x2 WHERE x1.ai=x2.ai;

# Estimate selectivity from ix1 or ix2.

--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1,t2 WHERE ai=bi;

# Estimate selectivity from ix2.

--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1,t2 WHERE a=bi;

--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t2 x1, t2 x2 WHERE x1.bi=x2.bi;

# Estimate selectivity from histogram on 'a'.

--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1,t2 WHERE ah=b;

--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1 x1, t1 x2 WHERE x1.ah=x2.ah;

# Estimate selectivity from histogram on 'a' or 'b'.

--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1,t2 WHERE ah=bh;

# Estimate selectivity from histogram on 'b'.

--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1,t2 WHERE a=bh;

--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t2 x1, t2 x2 WHERE x1.bh=x2.bh;

DROP TABLE num, t1, t2;

--echo #
--echo # Bug#34682561: Assertion `!eq_items.is_empty()' failed
--echo #               in make_join_hypergraph.cc
--echo #

CREATE TABLE t1 (f1 INTEGER, f2 INTEGER);
ANALYZE TABLE t1;
let $query =
SELECT f1 FROM t1
WHERE EXISTS (SELECT t2.f1
              FROM (t1 AS t2 JOIN t1 AS t3 ON (t3.f1 = t2.f2))
              LEFT JOIN t1 AS t4 ON TRUE
              WHERE t4.f1 = t3.f1 OR t3.f2 >= t2.f2)
GROUP BY f1;

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

DROP TABLE t1;

--echo #
--echo # Bug#34717171: Hypergraph :Assertion `false' failed
--echo #               in join_optimizer.cc
--echo #

CREATE TABLE t1 (pk INT PRIMARY KEY AUTO_INCREMENT, x INT);
CREATE TABLE t2 (x INT);

INSERT INTO t1 VALUES (), (), (), (), (), (), (), (), (), ();
INSERT INTO t2 VALUES (), (), (), (), (), (), (), (), (), ();

ANALYZE TABLE t1,t2;

let $query =
WITH subq AS (
  SELECT * FROM t2
  WHERE x IN (SELECT t1.pk FROM t1, t2 AS t3 WHERE t1.x = t3.x)
)
SELECT 1 FROM subq LEFT JOIN t2 AS t4 ON TRUE WHERE subq.x = t4.x;

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

DROP TABLE t1,t2;

--echo #
--echo # Bug#34828364: Assertion `!eq_items.is_empty()' failed
--echo #               in make_join_hypergraph.cc
--echo #

CREATE TABLE t1 (f1 INTEGER, f2 INTEGER);

ANALYZE TABLE t1;

let $query =
SELECT 1
FROM (SELECT * FROM t1
      WHERE f1 IN (SELECT t1.f1 FROM (t1 AS t2 JOIN t1 AS t3 ON t3.f1 = t2.f2)
                   LEFT JOIN t1 AS t4 ON TRUE
                   WHERE (t3.f2 <> t3.f2 OR t4.f2 = t2.f2))) AS t5 JOIN t1 AS t6
ON TRUE;

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

DROP TABLE t1;

--echo #
--echo # Bug#34821222: Hypergraph: mysqld crash-signal 11 - IsAnd &
--echo #               CommonSubexpressionElimination
--echo #

CREATE TABLE t1 (x INTEGER NOT NULL);
CREATE TABLE t2 (y INTEGER, z INTEGER);

SELECT 1 IN (
  SELECT COUNT(*) FROM t1 WHERE x NOT IN (
    SELECT 1 FROM t2 WHERE y <> y OR z <> z));

DROP TABLE t1, t2;

--echo #
--echo # Bug#34854369: Customer query hits assert(m_pq.is_valid()) failure
--echo #

# Graph simplification used to hit an assertion as a result of
# division by zero caused by information schema tables with zero row
# estimates.
CREATE TABLE t (table_id BIGINT UNSIGNED);
SELECT /*+ SET_VAR(optimizer_max_subgraph_pairs = 1) */ 1
FROM t AS t1 JOIN t AS t2 USING (table_id)
     JOIN INFORMATION_SCHEMA.INNODB_TABLES AS t3 USING (table_id)
     JOIN INFORMATION_SCHEMA.INNODB_TABLES AS t4 USING (table_id)
     JOIN INFORMATION_SCHEMA.INNODB_TABLES AS t5 USING (table_id)
     JOIN INFORMATION_SCHEMA.INNODB_TABLES AS t6 USING (table_id)
     JOIN INFORMATION_SCHEMA.INNODB_TABLES AS t7 USING (table_id)
     JOIN INFORMATION_SCHEMA.INNODB_TABLES AS t8 USING (table_id);
DROP TABLE t;

# Graph simplification used to hit an assertion as a result of
# division by zero caused by zero row estimates from MyISAM. (InnoDB
# never gives zero row estimates, not even for empty tables, whereas
# MyISAM does.)
CREATE TABLE t0 (x INT) ENGINE = MyISAM;
CREATE TABLE t1 (x INT) ENGINE = InnoDB;
SELECT /*+ SET_VAR(optimizer_max_subgraph_pairs = 1) */ 1
FROM t0 AS a NATURAL JOIN
     t0 AS b NATURAL JOIN
     t0 AS c NATURAL JOIN
     t0 AS d NATURAL JOIN
     t0 AS e NATURAL JOIN
     t0 AS f NATURAL JOIN
     t1 AS g NATURAL JOIN
     t1 AS h;
DROP TABLE t0, t1;


--echo #
--echo # Bug#34861693: Assertion
--echo # `std::abs(1.0 - EstimateAggregateRows(child, query_block, path->aggreg
--echo #

CREATE TABLE num (n INT);
INSERT INTO num VALUES (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);

CREATE TABLE t1 (a INT, b INT);

INSERT INTO t1 SELECT n,n FROM num UNION SELECT n+10,n+10 FROM num;

CREATE TABLE t2 (a INT, b INT);

ANALYZE TABLE t1, t2;

# The row estimate for "x1 LEFT JOIN (x2 LEFT JOIN x3)" may be different from that of
#  "(x1 LEFT JOIN x2) LEFT JOIN x3" (see bug #33550360 "Inconsistent row estimates
# in the hypergraph optimizer"). Then the the row estimate for GROUP BY will also depend
# on the join order. This triggers the assert (i.e. bug#34861693).
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT SQL_BIG_RESULT x1.a+0 k, COUNT(x1.b) FROM t1 x1
  LEFT JOIN t2 x2 ON x1.b=x2.a
  LEFT JOIN t1 x3 ON x2.b=x3.a GROUP BY k;

DROP TABLE t1,t2,num;


--echo #
--echo # Bug#35000554: assertion error in EstimateAggregateNoRollupRows()
--echo #

CREATE TABLE num10 (n INT);
INSERT INTO num10 VALUES (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);

CREATE TABLE t1(a INT, b INT, c INT);

ANALYZE TABLE t1 UPDATE HISTOGRAM ON a, b, c;

INSERT INTO t1 SELECT NULL, x1.n+x2.n*10, NULL FROM num10 x1, num10 x2;
INSERT INTO t1 VALUES (NULL, 0, 0);

ANALYZE TABLE t1;

# Row estimate should not be zero, even if histogram was built on empty table.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT b FROM t1 GROUP BY b;

# Row estimate should not be zero, even if histogram was built on empty table.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT b FROM t1 WHERE b=c;

ANALYZE TABLE t1 UPDATE HISTOGRAM ON a, b, c;

# Prior to the fix, this would trigger the assert, as we would estimate
# zero distinct values for 'a'.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT a,b FROM t1 GROUP BY a,b;

# Now there is an updated histogram (built on a non-empty table),
# and thus a better estimate.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT b FROM t1 WHERE b=c;

# Estimate should be two rows (NULL and 0).
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT c FROM t1 GROUP BY c;

DROP TABLE num10, t1;

CREATE TABLE t2(a INT, b INT);

INSERT INTO t2 VALUES (0, 0), (0, 1), (1, 2), (NULL, 3), (NULL, 4), (NULL, 5);

ANALYZE TABLE t2 UPDATE HISTOGRAM ON a, b;
ANALYZE TABLE t2;

# Estimate should be 1.5 rows (i.e. 25% of the table), as there are two distinct
# values, and 'a' is NULL for 50% of the rows.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t2 WHERE a=b;

DROP TABLE t2;

--echo #
--echo # Bug#35129863 Hypergraph: Multi-field indexes ignored in some
--echo # selectivity estimates
--echo #

CREATE TABLE num10 (n INT PRIMARY KEY);
INSERT INTO num10 VALUES (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
ANALYZE TABLE num10;

CREATE TABLE t1(
  a INT,
  b INT,
  c INT,
  d INT,
  e INT,
  f INT,
  g INT,
  h INT,
  v VARCHAR(5),
  PRIMARY KEY(a,b,c),
  KEY k1 (e,f,g),
  UNIQUE KEY k2(h)
);

INSERT INTO t1
  SELECT k%25, k%50, k, k, k%25, k%50, k, k, CAST( k%25 AS CHAR(5))
  FROM (select x1.n*10+x2.n k from num10 x1, num10 x2) d1;

ANALYZE TABLE t1 UPDATE HISTOGRAM ON a, b,c,d,e,f,g,v;
ANALYZE TABLE t1;

# Since [a,b] is a prefix of the primary key, we use the selectivity of this prefix instead
# of multiplying individual sekectivities of each field.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT * FROM t1 x1, t1 x2 WHERE x1.a=x2.a AND x1.b=x2.b;

# [a,b,c] is also a prefix of the primary key.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT * FROM t1 x1, t1 x2, t1 x3
  WHERE x1.a=x2.a AND x1.b=x2.b AND x2.c=x3.c AND x2.d=x3.d;

# Prefix of k1.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT * FROM t1 x1, t1 x2 WHERE x1.e=x2.e AND x1.f=x2.f;

# Prefix of k1.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT * FROM t1 x1, t1 x2 WHERE x1.e=x2.e AND x1.f=x2.f AND x1.g=x2.g;

# 'a' and 'e' are prefixes of two separate keys, so we multiply the selectivity of each.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT * FROM t1 x1, t1 x2 WHERE x1.a=x2.a AND x1.e=x2.e;

# [a,b] and [e,f] are index prefixes. Multiply the selectivity of each prefix.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT * FROM t1 x1, t1 x2 WHERE x1.a=x2.a AND x1.b=x2.b
   AND x1.e=x2.e AND x1.f=x2.f;

# [x2.a, x2.b] form an index prefix, but they are joined with different tables
# (x1 and x3). Therefore we derive the selectivity for x2.a from the
# primary key, and the selectivty of x2.b from the histogram for that field.
# And then we multiply these selectivities.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT * FROM t1 x1, t1 x2, t1 x3 WHERE x1.a=x2.a AND x2.b=x3.b;

# Mix of field=field and field=constant
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT * FROM t1 x1, t1 x2 WHERE x1.a=x2.a AND x1.b=8;

--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT * FROM t1 x1, t1 x2 WHERE x1.a=x2.a AND x1.b=x2.b
   AND x1.c=8;

--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT * FROM t1 x1, t1 x2 WHERE x1.a=x2.a AND x1.b=7
   AND x1.c=8;

# Join on entire primary key.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT * FROM t1 x1, t1 x2 WHERE x1.a=x2.a AND x1.b=x2.b
   AND x1.c=x2.c;

# field=field on single table.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT * FROM t1 WHERE a=b;

--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT * FROM t1 WHERE a=b AND c=d;

# Cycle x1->x2->x3 in predicate.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE FORMAT=TREE SELECT * FROM t1 x1, t1 x2, t1 x3
  WHERE x1.a=x2.a AND x2.b=x3.b AND x3.c=x1.c;

# Cap on most selective unique key (k2).
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT * FROM t1 JOIN num10 ON h=n;

# Cap on unique key k2 takes priority over [t1.a,t1.b] index prefix.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT * FROM t1 x1, t1 x2, t1 x3 WHERE
  x1.a=x2.a AND x1.b=x2.b AND x1.b=x3.h;

--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT * FROM t1 x1, t1 x2, t1 x3
  WHERE x1.a=x2.c AND x1.b=x2.b AND x2.b=x3.b;

# x3.a is missing on the inner side of the left join. Therefore we use
# histograms rather than the [x3.a, x3.b] index prefix for finding the
# selectivity of x3.b=x1.d.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT * FROM t1 x1 LEFT JOIN
  (t1 x2 JOIN t1 x3 ON x2.a=x3.a AND x2.b=x3.b) ON x3.b=x1.d;

--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM  t1 x1
WHERE 3 IN (SELECT x2.b FROM t1 x2 LEFT JOIN t1 x3 ON x2.c=x3.a AND x2.d=x3.b);

# Implicit cast from VARCHAR to INT.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1 x1 JOIN t1 x2 ON x1.a=x2.v AND x1.b=x2.b;

--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1 x1, t1 x2, t1 x3
WHERE x1.a=x2.c AND x1.a=x3.v AND x1.b=x3.f;

CREATE TABLE t2(x INT, y INT, z INT, KEY (x, y), KEY(y, x));

INSERT INTO t2(x, y) VALUES (1, 1), (2, 2), (3, 3), (4, 4);

CREATE TABLE t3 AS SELECT * FROM t2;

ANALYZE TABLE t2, t3;

--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t2 JOIN t3 USING (x, y);

DROP TABLE t1, t2, t3, num10;

--echo #
--echo # Bug#34762651 Too high row estimates for DISTINCT
--echo #


CREATE TABLE t1(
       a INT PRIMARY KEY,
       b INT,
       KEY(b),
       c INT
);

INSERT INTO t1
WITH RECURSIVE qn(n) AS (SELECT 1 UNION ALL SELECT n+1 FROM qn WHERE n<100)
SELECT n,  n%5, n%7 FROM qn;

ANALYZE TABLE t1 UPDATE HISTOGRAM ON c;
ANALYZE TABLE t1;

--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT DISTINCT b FROM t1;

--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT DISTINCT c FROM t1;

--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT DISTINCT b,c FROM t1;

DROP TABLE t1;

--echo #
--echo # Bug#34763224 Hypergraph orders and-terms inefficiently in subquery
--echo #

CREATE TABLE num (n INT);
INSERT INTO num VALUES (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);

CREATE TABLE t1(
  a INT PRIMARY KEY,
  b VARCHAR(128),
  c INT
);

INSERT INTO t1 SELECT k, CAST(100+k AS CHAR(10)), k
FROM (SELECT x1.n+x2.n*10 AS k FROM num x1, num x2) d1;

ANALYZE TABLE t1 UPDATE HISTOGRAM ON b, c;
ANALYZE TABLE t1;

# 'c<70' is cheaper even if it is less selective, so evaluate that first.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT 1 FROM t1 x1 WHERE x1.c IN
(SELECT c FROM t1 x2 WHERE  b<"150" AND c<70);

--replace_regex $elide_costs
EXPLAIN  FORMAT=TREE SELECT 1 FROM t1 WHERE b<"150" AND c<70;

# Even if 'c<80' is cheaper, its selectivity is so low that it should be
# evaluated last.
--replace_regex $elide_costs
EXPLAIN  FORMAT=TREE SELECT 1 FROM t1 WHERE b<"150" AND c<80;

--echo #
--echo # BUG 35507456
--echo # Assertion `[&]() { for (const Predicate *first = begin; first < end; first++)
--echo #
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT 1 FROM t1
WHERE  b<"150" AND RAND(0)>-1 AND c< 70;

DELIMITER $$;
CREATE FUNCTION foo(i INT)
  RETURNS INT
  LANGUAGE SQL
BEGIN
  RETURN i+1;
END $$
DELIMITER ;$$

# Function call should be evaluated last.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT 1 FROM t1 WHERE foo(2)=3 AND c=5;

DROP FUNCTION foo;
DROP TABLE num, t1;

--echo #
--echo # Bug#36316088 	Mysqld crash - Assertion `false &&
--echo # "Inconsistent row counts for differ
--echo #

CREATE TABLE t1(
  a INT,
  b INT
);

INSERT INTO t1 values (1,1),(2,2);
ANALYZE TABLE t1;

CREATE FUNCTION func(x INT) RETURNS INTEGER NO SQL RETURN x+1;

# The term with a function call should be evaluated last.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 x1, t1 x2
WHERE func(x1.b)=x2.b AND x1.a=x2.a;

DROP FUNCTION func;

DROP TABLE t1;

--echo #
--echo # Bug#34787357 Hypergraph: row estimates for field=non_field_term
--echo # ignores indexes and histogram
--echo #

CREATE TABLE t1 (
  a INT,
  b INT,
  c INT,
  d INT,
  e INT,
  f INT,
  g INT,
  PRIMARY KEY(a),
  KEY k1 (b,d), --  'b' and 'd' are indepdendent.
  KEY k2 (b,e), -- 'b' is funtionally dependent on 'e'.
  KEY k3 (b,g), -- 'b' is funtionally dependent on 'g'.
  KEY k4 (f,g) -- 'f' and 'g' are independet.
);

INSERT INTO t1
WITH RECURSIVE qn(n) AS
(SELECT 0 UNION ALL SELECT n+1 FROM qn WHERE n<255)
SELECT n AS a, n DIV 16 AS b, n % 16 AS c, n % 16 AS d, n DIV 8 AS e,
  n % 32 AS f, n DIV 8 AS g FROM qn;

ANALYZE TABLE t1 UPDATE HISTOGRAM ON c;
ANALYZE TABLE t1;

# We estimate selectivity of <field>=<independent expression> as
# 1/<num distinct values of field>. If there is a histogram on <field>,
# or if it is the first field of an (non-hash) index, then the (estimated)
# number of distinct values is directly available. If it is the second or
# subsequent field of an index, we do not know to what extent <field> is
# correlated with the preceeding fields. In this case, we estimate the
# selectivity as the geometric mean of the two extermes:
# 1) Uncorrelated:
#    selectivity = records_per_key(prefix+key) / rows_in_table
#
# 2) prefix is functionally dependent on field:
#    selectivity = records_per_key(prefix+key) / records_per_key(prefix)

# Make estimate from index, even if value is unknown.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1 WHERE b=FLOOR(RAND(0));

# Make estimate from histogram, even if value is unknown.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1 WHERE c=FLOOR(RAND(0));

# Make estimate from histogram, even if value is unknown. Note that the field
# is on the right hand side of '='.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1 WHERE FLOOR(RAND(0))=c;

# Make estimate from second field of index. Since 'b' and 'd' are independent,
# the estimate will be too low.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1 WHERE d=FLOOR(RAND(0));

# Make estimate from second field of index. Since 'b' in functionally dependent
# on 'e', the estimate will be too high.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1 WHERE e=FLOOR(RAND(0));

# Make estimate from second field of index. Since 'b' and 'd' are independent,
# the estimate will be too low.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT /*+ SKIP_SCAN(t1) */ 1 FROM t1 WHERE d=0;

# Make estimate from second field of index. Since 'b' in functionally dependent
# on 'e', the estimate will be too high.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT /*+ SKIP_SCAN(t1) */ 1 FROM t1 WHERE e=0;

# Make estimate from second field of indexes k3 and k4.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1 WHERE g=FLOOR(RAND(0));

# Make estimate from index, even if value is unknown.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1 WHERE b=(SELECT MIN(b) FROM t1);

# Make estimate from histogram, even if value is unknown.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1 WHERE c=(SELECT MIN(b) FROM t1);

# Use histogram or index to make estimate for <field>!=<expression>
# predicates.

# Make estimate from index, even if value is unknown.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1 WHERE b<>FLOOR(RAND(0));

# Make estimate from histogram, even if value is unknown.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1 WHERE c<>FLOOR(RAND(0));

# Make estimate from histogram, even if value is unknown. Note that the field
# is on the right hand side of '<>'.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1 WHERE FLOOR(RAND(0))<>c;

# Make estimate from index, even if value is unknown.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1 WHERE b<>(SELECT MIN(b) FROM t1);

# Make estimate from histogram, even if value is unknown.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1 WHERE c<>(SELECT MIN(b) FROM t1);

# Make estimate from second field of index.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1 WHERE d<>(SELECT MIN(b) FROM t1);

# Make estimate from second field of index.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT 1 FROM t1 WHERE e<>(SELECT MIN(b) FROM t1);

# Subquery with column resolved in outer reference (OUTER_REF_TABLE_BIT).
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE FORMAT=TREE SELECT * FROM t1 x1 WHERE d =
(SELECT MIN(g) FROM t1 x2 WHERE x1.b<>x2.b);

DROP TABLE t1;


--echo # Bug#35439787 Assertion `false && "Inconsistent row counts for
--echo # different AccessPath objects."'
--echo #

CREATE TABLE t1 (
  a INT PRIMARY KEY,
  b INT,
  c INT,
  d INT,
  e INT,
  KEY k1 (b,c)
);

ANALYZE TABLE t1;

# To reproduce this bug we use a query with:
# - A top-level semi-join, so that we get one CompanionSet for the
#   root and one for the inner-join.
# - A cyclic dependency between x2, x3 and x4.
# - Terms comparing both the first and second field in a key to
#   other fields.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT 1 FROM t1 x1 WHERE EXISTS
(SELECT 1 FROM t1 x2, t1 x3, t1 x4
  WHERE x2.b=x3.d AND x2.c=x3.e AND x3.d=x4.b AND x2.e=x4.e);

DROP TABLE t1;

# Test case for post-push fix for commit with
# 'Change-Id: I40156663f32a407490b066a566abe72b356cfda9'. Fix for UBSAN
# error: building reference to dereferenced null pointer.
CREATE TABLE t1 (a int, b int);

ANALYZE TABLE t1;

--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM  t1 x1 LEFT JOIN (t1 x2, t1 x3)
ON x1.a=x3.a WHERE x1.b<x2.b OR x2.a IS NULL;

DROP TABLE t1;

--echo #
--echo # Bug#35719688 Assertion`new_path->cost >= new_path->init_cost' in
--echo # ExpandSingleFilterAccessPath
--echo #

CREATE TABLE t1 (
  a INT,
  b INT,
  c INT,
  PRIMARY KEY(a),
  KEY k_b(b),
  KEY k_c(c)
);

INSERT INTO t1 VALUES (1,1,1);

ANALYZE TABLE t1;

CREATE TABLE t2 (
  a INT PRIMARY KEY,
  b INT
);

INSERT INTO t2 WITH RECURSIVE qn(n) AS
(SELECT 1 UNION ALL SELECT n+1 FROM qn WHERE n<50) SELECT n, n FROM qn;

ANALYZE TABLE t2;

# To trigger this bug we need a query with:
# - A ROWID_UNION access path.
# - A predicate giving a selectivity estmate of zero (prior to the fix).
# - An expensive filter condition.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 x0 WHERE (b=3 OR c=4) AND a <>
(SELECT MAX(x1.a+x2.a) FROM t2 x1 JOIN t2 x2 ON x1.b<x2.b);

DROP TABLE t1,t2;

--echo #
--echo # Bug#35789967 Assertion `val >= 0.0 || val == kUnknownCost' failed.
--echo #

CREATE TABLE t1
(
  a INT,
  b INT,
  c INT,
  PRIMARY KEY(a),
  KEY k2 (b,c)
);

INSERT INTO t1
WITH RECURSIVE qn(n) AS (SELECT 10 UNION ALL SELECT n-1 FROM qn WHERE n>0)
SELECT n, 1, n FROM qn;

ANALYZE TABLE t1;

# The assert was triggered by an independent singlerow subselect as a filter
# condition.

--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT /*+ SKIP_SCAN(x1) */ b FROM t1 x1
  WHERE c < 1 AND c = (SELECT MAX(b) FROM t1 x2);

DROP TABLE t1;

--echo #
--echo # Bug#35790381 Assertion `false &&
--echo # "Inconsistent row counts for different AccessPath objects."'
--echo #

CREATE TABLE t1 (a INT, b INT, KEY k1 (a));

INSERT INTO t1 VALUES (1,1),(2,2),(1,1),(2,2),(1,1),(2,2),(1,1),(2,2),(1,1),(2,2),(1,1),(2,2);

ANALYZE TABLE t1 UPDATE HISTOGRAM ON b;
ANALYZE TABLE t1;

# The code that made row estimates for aggregation did not understand that
# 'a' was a field, since it was represented by an Item_ref rather than an
# Item_field. Therefore, it ignored the k1 index, and used a rule of thumb
# instead (the square root of the number of input rows).
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT a, COUNT(*) FROM
  (SELECT x1.a FROM t1 x1, t1 x2 WHERE x1.b = x2.a) AS dt GROUP BY a;

DROP TABLE t1;

--echo #
--echo # Bug#35483044 Hypergraph: Invalid row estimate for filter on
--echo # 'Index range scan'
--echo #

CREATE TABLE t1 (
  a INT PRIMARY KEY,
  b INT NOT NULL,
  c INT,
  KEY k_b(b),
  KEY k_c(c)
);

INSERT INTO t1 WITH RECURSIVE qn(n) AS
(SELECT 1 UNION ALL SELECT n+1 FROM qn WHERE n<30) SELECT n, n/2, n/2 FROM qn;

ANALYZE TABLE t1;

# c=NULL is always false and should not affect the row estimate.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 WHERE b=5 OR c=NULL;

# 'c<=>NULL' and 'c IS NULL' are equivalent and should get the same row
# estimate.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 WHERE b=5 OR c<=>NULL;

--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 WHERE b=5 OR c IS NULL;

# b=NULL is always false and should not affect the row estimate.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 WHERE b=NULL OR c=5;

# b<=>NULL is always false and should not affect the row estimate.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 WHERE b<=>NULL OR c=5;

# Use index for selectivity estimate.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 WHERE b<=>FLOOR(RAND(0));

# Use index for selectivity estimate.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 WHERE c<=>FLOOR(RAND(0));

ANALYZE TABLE t1 UPDATE HISTOGRAM ON b,c;

# c=NULL is always false and should not affect the row estimate.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 WHERE b=5 OR c=NULL;

# 'c<=>NULL' and 'c IS NULL' are equivalent and should get the same row
# estimate (from the histogram).
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 WHERE b=5 OR c<=>NULL;

--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 WHERE b=5 OR c IS NULL;

# b=NULL is always false and should not affect the row estimate.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 WHERE b=NULL OR c=5;

# b<=>NULL is always false and should not affect the row estimate.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 WHERE b<=>NULL OR c=5;

# Use index for selectivity estimate.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 WHERE b<=>FLOOR(RAND(0));

# Use index for selectivity estimate.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 WHERE c<=>FLOOR(RAND(0));

DROP TABLE t1;

--echo #
--echo # Bug#35898221 Hypergraph: too low row estmate for semijoin
--echo # that is not an equijoin
--echo #

CREATE TABLE t1 (
  a INT PRIMARY KEY,
  b INT
);

CREATE TABLE t2 (
  k INT,
  l INT,
  PRIMARY KEY(k)
);

INSERT INTO t1 WITH RECURSIVE qn(n) AS
(SELECT 0 UNION ALL SELECT n+1 FROM qn WHERE n<29) SELECT n, n FROM qn;

INSERT INTO t2 WITH RECURSIVE qn(n) AS
(SELECT 0 UNION ALL SELECT n+1 FROM qn WHERE n<19) SELECT n, n%10 FROM qn;

ANALYZE TABLE t1 UPDATE HISTOGRAM ON b;
ANALYZE TABLE t2 UPDATE HISTOGRAM ON l;
ANALYZE TABLE t1,t2;

# Simple semijoin. The row estimate should be:
# CARD(t1) * CARD("SELECT DISTINCT l FROM t2") * SELECTIVITY(t1.a=t2.l)
# = 30 * 10 * 1/30 = 10.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT * FROM t1 WHERE t1.a IN (SELECT t2.l FROM t2);

# Semijoin that is not an equijoin.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT * FROM t1 WHERE EXISTS
(SELECT 1 FROM t2 WHERE t1.a=t2.l AND t1.b<>t2.k);

# Semijoin refering same inner field ('l') multiple times.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT * FROM t1 WHERE EXISTS
(SELECT 1 FROM t2 WHERE t2.l+ABS(t2.l)=t1.a);

# Semijoin refering multiple inner fields.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT * FROM t1 WHERE EXISTS
(SELECT 1 FROM t2 WHERE t2.k+t2.l=t1.a);

# Simple antijoin.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT * FROM t1 WHERE NOT EXISTS
(SELECT 1 FROM t2 WHERE t1.a=t2.l);

# Antijoin that is not an equijoin.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT * FROM t1 WHERE NOT EXISTS
(SELECT 1 FROM t2 WHERE t1.a=t2.l AND t1.b<>t2.k);

# Semijoin with row estimate from inner operand less than one.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 WHERE a IN
(SELECT t2.l FROM t2 WHERE t2.l<0);

# Semijoin with constant inner operand.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 WHERE t1.a IN (SELECT 5 FROM t2);

DROP TABLE t1,t2;


--echo #
--echo # Bug#35997316 Hypergraph: Incomplete and inconvenient optimizer trace
--echo #

CREATE TABLE t1 (a INT PRIMARY KEY, b INT);

INSERT INTO t1 WITH RECURSIVE qn(n) AS
(SELECT 0 UNION ALL SELECT n+1 FROM qn WHERE n+1<30) SELECT n, n%10 FROM qn;

ANALYZE TABLE t1 UPDATE HISTOGRAM ON b;
ANALYZE TABLE t1;

SET SESSION OPTIMIZER_TRACE='enabled=on';

--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 x1
  WHERE x1.a IN (SELECT x2.b FROM t1 x2);

# Check that how we estimate the number of distinct values for x2.b are
# logged in the optimizer trace.
SELECT REGEXP_SUBSTR(
  trace, "distinct values for field 'b' from histogram[^,]*",1,1,'n')
  FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;

SET SESSION OPTIMIZER_TRACE='enabled=off';

DROP TABLE t1;

--echo #
--echo # Bug#36125903 Hypergraph: Inconsistent numeric precision for
--echo # AccessPath in optimizer trace
--echo #

CREATE TABLE t1 (
  a INT PRIMARY KEY,
  b INT
);

INSERT INTO t1 WITH RECURSIVE qn(n) AS
(SELECT 0 UNION ALL SELECT n+1 FROM qn WHERE n<100) SELECT n, n FROM qn;

ANALYZE TABLE t1 UPDATE HISTOGRAM ON b;
ANALYZE TABLE t1;

SET SESSION OPTIMIZER_TRACE="enabled=on";

--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 x1, t1 x2, t1 x3, t1 x4;

SELECT REGEXP_SUBSTR(trace, " - \\{HASH_JOIN[^\n]*keeping",1,1,'n')
  FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;

--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 WHERE a=5 AND b=5;

SELECT REGEXP_SUBSTR(trace, " - \\{EQ_REF[^\n]*keeping",1,1,'n')
  FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;

SET SESSION OPTIMIZER_TRACE="enabled=off";

DROP TABLE t1;

--echo #
--echo # Bug#33502694 Hypergraph optimizer trace does not respect memory limits
--echo #

CREATE TABLE t1(
  pk INT PRIMARY KEY,
  a INT,
  KEY ka(a),
  b INT,
  KEY kb(b)
);

INSERT INTO t1
WITH RECURSIVE qn(n) AS (SELECT 1 UNION ALL SELECT n+1 FROM qn WHERE n<100)
SELECT n, n%11, n%11 FROM qn;

ANALYZE TABLE t1;

SET @old_optimizer_trace_max_mem_size=@@optimizer_trace_max_mem_size;
SET SESSION optimizer_trace_max_mem_size=16384;
SET SESSION OPTIMIZER_TRACE='enabled=on';

# Plan a query that generates more than 16kB of optimizer trace.
EXPLAIN FORMAT=JSON INTO @explain_output
SELECT a FROM t1 x2 WHERE x2.b IN
  (SELECT a FROM t1 x1 WHERE x1.b IN (SELECT a FROM t1 x0));

# The exact figures are platform dependent, so check that they are within a
# certain range. It is also possible that more optimizer trace is added in
# later patches, and that the expected length has to be changed accordingly.
SELECT
  CAST(LENGTH(TRACE) AS DOUBLE) / @@optimizer_trace_max_mem_size
  BETWEEN 0.9 AND 1.1 AS trace_length_ok,
  MISSING_BYTES_BEYOND_MAX_MEM_SIZE BETWEEN 10000 AND 30000 AS excess_length_ok
FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;


SET SESSION optimizer_trace_max_mem_size=@old_optimizer_trace_max_mem_size;
SET SESSION OPTIMIZER_TRACE='enabled=off';

DROP TABLE t1;

--echo # Bug#5889990: Setting secondary_engine to OFF causes offload issues

CREATE TABLE t(x INT, y INT);
INSERT INTO t VALUES (1, 2), (2, 3);
ANALYZE TABLE t;

SET optimizer_switch='hypergraph_optimizer=off';

PREPARE ps FROM
'SELECT *
 FROM t AS t1 LEFT JOIN t AS t2 ON t1.x=t2.x AND t1.y IN (SELECT x FROM t)';

EXECUTE ps;

SET optimizer_switch='hypergraph_optimizer=on';

EXECUTE ps;

SET optimizer_switch='hypergraph_optimizer=off';

EXECUTE ps;

SET optimizer_switch='hypergraph_optimizer=on';

PREPARE ps FROM
'SELECT *
 FROM t AS t1 LEFT JOIN t AS t2 ON t1.x=t2.x AND t1.y IN (SELECT x FROM t)';

EXECUTE ps;

SET optimizer_switch='hypergraph_optimizer=off';

EXECUTE ps;

DROP TABLE t;

CREATE TABLE t(x VARCHAR(100), FULLTEXT KEY (x));
INSERT INTO t VALUES ('abc'), ('xyz'), ('abc abc');
ANALYZE TABLE t;

SET optimizer_switch='hypergraph_optimizer=on';

PREPARE ps FROM
'SELECT x, MATCH(x) AGAINST (''abc'') AS score FROM t
 GROUP BY x HAVING MATCH(x) AGAINST(''abc'') > 0';

EXECUTE ps;

SET optimizer_switch='hypergraph_optimizer=off';

EXECUTE ps;

SET optimizer_switch='hypergraph_optimizer=on';

EXECUTE ps;

SET optimizer_switch='hypergraph_optimizer=off';

PREPARE ps FROM
'SELECT x, MATCH(x) AGAINST (''abc'') AS score FROM t
 GROUP BY x HAVING MATCH(x) AGAINST(''abc'') > 0';

EXECUTE ps;

SET optimizer_switch='hypergraph_optimizer=on';

EXECUTE ps;

DROP TABLE t;

--echo #
--echo # Bug #36135001: Hypergraph: Too low row estimate for index lookup
--echo #

CREATE TABLE t1(
  a INT,
  b INT,
  c INT,
  d INT,
  PRIMARY KEY(a),
  KEY k1(d)
);

INSERT INTO t1
WITH RECURSIVE qn(n) AS
(SELECT 0 UNION ALL SELECT n+1 FROM qn WHERE n<19)
SELECT n, n%10, n%10, n%10 FROM qn;

ANALYZE TABLE t1 UPDATE HISTOGRAM ON b,c;
ANALYZE TABLE t1;

# Do a lookup on the primary key, so that we can verify that the row estimate
# is 1.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 x1, t1 x2, t1 x3
  WHERE x1.b=x2.a AND x2.b=x3.a AND (x1.c < 5 OR x3.c=7) LIMIT 1;

# Do a lookup on non-unique key k1, where the row estimate should be 2.
# 'ORDER BY' ensures that we get a nested loop join, where the lookup is
# present.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 x1, t1 x2, t1 x3
  WHERE x1.b=x2.d AND x2.b=x3.d AND (x1.c < 5 OR x3.c=7) ORDER BY x3.d LIMIT 1;

# Check that we handle two identical conditions correctly.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 x1 LEFT JOIN t1 x2
  ON x1.a <=> x2.a AND  x1.a <=> x2.a;

DROP TABLE t1;

--echo #
--echo # Bug#35855573 Assertion `false && "Inconsistent row counts for
--echo # different AccessPath objects."'
--echo #

CREATE TABLE t(pk INT PRIMARY KEY AUTO_INCREMENT, x INT);
INSERT INTO t VALUES (), (), (), (), (), (), (), (), (), (), (), ();
ANALYZE TABLE t;

# Since t1.pk = t2.x and pk is the primary key, there is a functional
# dependency t2.x->t1.x. Therefore, some candidate plans group on {t2.x, t1.x}
# and others on {t1.x}, which are both valid. But EstimateDistinctRows() will
# give different row estimates for these two sets of fields, as it does not take
# functional depdenencies into account. This triggered the assert.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT SQL_BIG_RESULT DISTINCT t1.x, t2.x FROM t AS t1, t AS t2
  WHERE t1.pk = t2.x ORDER BY t2.x;
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT SQL_SMALL_RESULT DISTINCT t1.x, t2.x FROM t AS t1, t AS t2
  WHERE t1.pk = t2.x ORDER BY t2.x;

--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT SQL_BIG_RESULT DISTINCT t1.x, t1.pk FROM t AS t1, t AS t2
  WHERE t1.pk = t2.x ORDER BY t1.x;
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT SQL_SMALL_RESULT DISTINCT t1.x, t1.pk FROM t AS t1, t AS t2
  WHERE t1.pk = t2.x ORDER BY t1.x;

--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT SQL_BIG_RESULT DISTINCT t1.pk, t2.x FROM t AS t1, t AS t2
  WHERE t1.pk = t2.x ORDER BY t2.x;
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT SQL_SMALL_RESULT DISTINCT t1.pk, t2.x FROM t AS t1, t AS t2
  WHERE t1.pk = t2.x ORDER BY t2.x;

DROP TABLE t;

--echo #
--echo # Bug#35991881 Hypergraph: Wrong init_cost for nested loop join
--echo #

CREATE TABLE t1 (
  a INT PRIMARY KEY,
  b INT,
  c INT
);

INSERT INTO t1 WITH RECURSIVE qn(n) AS
  (SELECT 0 UNION ALL SELECT n+1 FROM qn WHERE n+1<20)
  SELECT n, n%10, n%10 FROM qn;

ANALYZE TABLE t1 UPDATE HISTOGRAM ON b, c;
ANALYZE TABLE t1;

# This query should produce a plan with a NLJ between x1 and d1, since we
# order on an indexed column (x1.a) and have LIMIT.
# The relatively high first-row cost for d1 should be included in the
# first-row cost of that NLJ.
# Note: The plan depends on the cost model and is no longer stable.
# We likely need a way to fix a specific plan to make this test maintainable/valid.
# Look into adding hints to get the desired plan after WL#16006.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 x1 JOIN
  (SELECT MAX(x2.c) k FROM t1 x2 JOIN t1 x3 ON x2.c < x3.b GROUP BY x3.c) d1
  ON x1.b=d1.k ORDER BY x1.a LIMIT 1;

DROP TABLE t1;

--echo #
--echo # Bug#36032958: Assertion `IsEmpty(child.delayed_predicates)' failed
--echo #

CREATE TABLE t(x INT, KEY(x));
SELECT ROW_NUMBER() OVER () FROM t WHERE x = RAND() GROUP BY x;
DROP TABLE t;

--echo #
--echo # Bug#35989799 Discontinuity in row estimate for ROLLUP in hypergraph
--echo #

CREATE TABLE t1(x INT, y INT);

INSERT INTO t1 VALUES (1,1), (2,2);

ANALYZE TABLE t1;

# The row estimate before aggregation is n where n < 1. The estimated
# number of aggregated rows is then also n, and the total estimate,
# including the two rollup rows should then be 1 + n * 2.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 WHERE x > 0 GROUP BY x, y WITH ROLLUP;

DROP TABLE t1;

--echo #
--echo # Bug#36099491: Assertion secondary_engine_cost_hook != nullptr
--echo #               failed with hypergraph_optimizer
--echo #

CREATE TABLE t (a INT, b VARCHAR(10));

# Make the base tables bigger, so that it looks tempting to join the
# smaller lateral derived table first in the queries below. The
# optimizer needs to resist the temptation, because the lateral
# derived table cannot be joined before the table it depends on.
INSERT INTO t (a)
WITH RECURSIVE 150tup(n) AS
  (SELECT 1 UNION ALL SELECT n + 1 FROM 150tup WHERE n < 150)
SELECT n FROM 150tup;

ANALYZE TABLE t;

SELECT /*+ SET_VAR(optimizer_max_subgraph_pairs = 1) */ COUNT(*)
FROM t AS t1 LEFT JOIN t AS t2 ON TRUE
WHERE t1.a IN (SELECT * FROM (SELECT DISTINCT t2.a) AS t3);

# This query is quite slow to execute, since it executes the lateral
# derived table once per row in the t1×t2 Cartesian product. (Which is
# why the graph simplifier wanted to join in the derived table with t1
# before the t1-t2 join.) To speed it up, we only EXPLAIN it, which is
# sufficient to show the bug. We don't care which plan it chooses,
# only that it is able to find one, which it wasn't able to before.
--disable_result_log
EXPLAIN FORMAT=TREE
SELECT /*+ SET_VAR(optimizer_max_subgraph_pairs = 1) */ COUNT(*)
FROM t AS t1 LEFT JOIN t AS t2 ON TRUE,
     LATERAL (SELECT DISTINCT t2.a FROM t) AS t3
WHERE t1.a = t3.a;
--enable_result_log

SELECT /*+ SET_VAR(optimizer_max_subgraph_pairs = 1) */ COUNT(*)
FROM t AS t1 LEFT JOIN t AS t2 ON 1,
     JSON_TABLE(t2.b, '$[*]' COLUMNS(i INT PATH '$[0]')) AS t3
WHERE t3.i < 10 AND t1.a = t3.i;

DROP TABLE t;

--echo #
--echo # Bug#36098954: Assertion GraphSimplifier::EdgesAreNeighboring
--echo #               failed with hypergraph_optimizer
--echo #

CREATE TABLE t(x INT);
SELECT /*+ SET_VAR(optimizer_max_subgraph_pairs = 1) */ 1
FROM t WHERE x IN (
  SELECT NULL FROM t AS t1, t AS t2 STRAIGHT_JOIN t AS t3 ON t2.x = t3.x
) OR x = 1;
DROP TABLE t;

--echo #
--echo # Bug#37272285 Assertion `false && "Inconsistent row counts for
--echo # different AccessPath objects."' failed.
--echo #

CREATE TABLE t1(x INT);
CREATE TABLE t2(x INT);
CREATE TABLE t3(id INT PRIMARY KEY);
CREATE TABLE t4(id INT NULL UNIQUE);

INSERT INTO t1 VALUES (1), (2), (3);
INSERT INTO t2 VALUES (1), (2), (3), (4);
INSERT INTO t3 VALUES (1), (2), (3);
INSERT INTO t4 VALUES (1), (2), (3);

ANALYZE TABLE t1, t2, t3, t4;

# Check that query does not trigger assert.
--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT * FROM t1 WHERE 1 IN
(SELECT t2.x+1 FROM t2, t3 WHERE t3.id = t1.x+t2.x);

# Check that query does not trigger assert.
--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT * FROM t1 LEFT JOIN LATERAL
(SELECT t2.x+1 k FROM t2, t3 WHERE t3.id = t1.x+t2.x) d2 ON 1=k;

# These two did not trigger the assert, but probably could have if we had
# implemented NLJ for <=> before fixing Bug#37272285 (cf. Bug#36955411
# "Hypergraph optimizer does not use index lookups for <=> predicates").
--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT * FROM t1 WHERE 1 IN
(SELECT t2.x+1 FROM t2, t3 WHERE t3.id <=> t1.x+t2.x);

--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT * FROM t1 LEFT JOIN LATERAL
(SELECT t2.x+1 k FROM t2, t3 WHERE t3.id <=> t1.x+t2.x) d2 ON 1=k;

# These two did not trigger the assert, but probably could have if we had
# implemented REF_OR_NULL for Hypergraph.

--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT * FROM t1 WHERE 1 IN
(SELECT t2.x+1 FROM t2, t4 WHERE t4.id = t1.x+t2.x or t4.id IS NULL);

--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT * FROM t1 LEFT JOIN LATERAL
(SELECT t2.x+1 k FROM t2, t4 WHERE t4.id = t1.x+t2.x or t4.id IS NULL) d2
ON 1=k;

DROP TABLE t1,t2,t3,t4;


--echo #
--echo # Bug#36955411 Hypergraph optimizer does not use index lookups
--echo # for <=> predicates
--echo #


CREATE TABLE t1(
  pk INT NOT NULL PRIMARY KEY,
  i1 INT,
  i2 INT,
  i3 INT,
  u INT,
  KEY k1 (i1),
  KEY k2 (i2, i3),
  UNIQUE KEY ku(u)
);

INSERT INTO t1
WITH RECURSIVE t(n) AS (
    SELECT 0 AS i UNION ALL
    SELECT n + 1 FROM t WHERE n + 1 < 50
)
SELECT n, n, n, n, n  FROM t;

INSERT INTO t1 VALUES (-1, NULL, NULL, NULL, NULL);

CREATE TABLE t2 (k INT, l INT);
INSERT INTO t2 VALUES (NULL, NULL), (1, 1);

ANALYZE TABLE t1, t2;

--echo # Impossible WHERE should give ZERO_ROW path.
--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT * FROM t1 WHERE t1.u IN  (1,2,3) AND t1.u IS NULL;

--echo # The following queries should map <=> and IS NULL to REF
--echo # on the k1 index.

--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT pk FROM t1 WHERE i1 IS NULL;
SELECT pk FROM t1 WHERE i1 IS NULL;

--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT pk FROM t1 WHERE i1 <=> 1;
SELECT pk FROM t1 WHERE i1 <=> 1;

--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT pk FROM t1 WHERE i1 <=> NULL;
SELECT pk FROM t1 WHERE i1 <=> NULL;

--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT COUNT(*) FROM t1, t2 WHERE t2.k <=> t1.i1;
SELECT COUNT(*) FROM t1, t2 WHERE t2.k <=> t1.i1;

--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT COUNT(*) FROM t1, t2 WHERE t1.i1 <=> t2.k+t2.l;
SELECT COUNT(*) FROM t1, t2 WHERE t1.i1 <=> t2.k+t2.l;

--echo # Prefer lookup with or without histogram on 'i'.
ANALYZE TABLE t1 UPDATE HISTOGRAM ON i;
--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT pk FROM t1 WHERE i1 <=> NULL;
SELECT pk FROM t1 WHERE i1 <=> NULL;

--echo # The following queries should map <=> and ISNULL to EQ_REF access
--echo # on the ku index.

--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT pk FROM t1 WHERE u IS NULL;
SELECT pk FROM t1 WHERE u IS NULL;

--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT pk FROM t1 WHERE u <=> 1;
SELECT pk FROM t1 WHERE u <=> 1;

--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT pk FROM t1 WHERE u <=> NULL;
SELECT pk FROM t1 WHERE u <=> NULL;

--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT COUNT(*) FROM t1, t2 WHERE t2.k <=> t1.u;
SELECT COUNT(*) FROM t1, t2 WHERE t2.k <=> t1.u;

--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE SELECT COUNT(*) FROM t1, t2 WHERE t1.u <=> t2.k+t2.l;
SELECT COUNT(*) FROM t1, t2 WHERE t1.u <=> t2.k+t2.l;

--echo # Row estimate should utilize the k2 index for both <=> and =.

EXPLAIN FORMAT=JSON INTO @plan1 SELECT * FROM t1, t2
WHERE t2.k <=> t1.i2 AND t2.l<=>t1.i3;

EXPLAIN FORMAT=JSON INTO @plan2 SELECT * FROM t1, t2
WHERE t2.k = t1.i2 AND t2.l=t1.i3;

SELECT ABS(JSON_EXTRACT(@plan1, '$.query_plan.estimated_rows') -
JSON_EXTRACT(@plan2, '$.query_plan.estimated_rows')) < 0.05 AS ok;

DROP TABLE t1,t2;

--echo #
--echo # Bug#36578613: Hypergraph result difference when querying I_S.TABLES
--echo #

# Create a table with a LONGTEXT column, to ensure that filesort uses
# row IDs, and with enough rows to make a hash join look more
# attractive than a nested loop join.
CREATE TABLE t(x INT, y LONGTEXT);
INSERT INTO t VALUES (1, 'a'), (2, 'b'), (3, 'c'), (4, 'd'), (5, 'e'), (6, 'f');
ANALYZE TABLE t;

let $query =
SELECT DISTINCT t1.x, t1.y
FROM t AS t1, t AS t2 WHERE t1.x = t2.x
ORDER BY -t1.x;

# To reproduce the bug, the plan should have two SORT nodes (one for
# ORDER BY and one for DISTINCT) on top of a HASH_JOIN node.
--eval EXPLAIN FORMAT=JSON INTO @explain $query
query_vertical
SELECT
  JSON_EXTRACT(@explain, '$.query_plan.access_type') AS sort_for_order_by,
  JSON_EXTRACT(@explain, '$.query_plan.inputs[0].access_type') AS sort_for_distinct,
  JSON_EXTRACT(@explain, '$.query_plan.inputs[0].inputs[0].join_algorithm') AS hash_join;

--eval $query

DROP TABLE t;

--echo #
--echo # Bug#36678321: Hypergraph: Sort-ahead doesn't provide row IDs to later
--echo #               sort that needs them.
--echo #

CREATE TABLE t1(int_col1 INT, int_col2 INT);
INSERT INTO t1 VALUES (1, 1), (2, 2), (3, 3), (4, 4), (5, 5), (6, 6);

CREATE TABLE t2 (int_col_key INT, longtext_col LONGTEXT, KEY (int_col_key));
INSERT INTO t2
  WITH RECURSIVE src(n) AS (
    SELECT 1 UNION ALL SELECT n + 1 FROM src WHERE n < 50
  )
  SELECT n, n FROM src;
INSERT INTO t2 SELECT * FROM t2;
INSERT INTO t2 SELECT * FROM t2;

ANALYZE TABLE t1, t2;

let $query =
SELECT SQL_BIG_RESULT DISTINCT t1.int_col2
FROM t1, t2 WHERE t1.int_col1 = t2.int_col_key
ORDER BY ANY_VALUE(t2.longtext_col);

# To reproduce the problem, we need to have a nested loop join with a
# sort-ahead on t1.int_col2 on the left side of the join, whose
# ordering is used for removing duplicates after the join, and a new
# sort for the ORDER BY clause on top. The query used to return the
# same value for all rows.
--replace_regex $elide_costs
--eval EXPLAIN FORMAT=TREE $query
--eval $query

DROP TABLE t1, t2;

--echo #
--echo # Bug#36715239: Assertion `found_a_plan || is_secondary_engine' failed.
--echo #
CREATE TABLE t(a INT, b INT, c INT);
INSERT INTO t VALUES (1, 2, 3), (2, 3, 4), (3, 4, 5), (4, 5, 6);
ANALYZE TABLE t;

let $query =
WITH s AS (
    SELECT b FROM t AS t2
    WHERE EXISTS (SELECT * FROM t AS t3 WHERE t2.b = t3.c OR t3.c = 0)
)
SELECT t1.a, s.b FROM t AS t1 LEFT JOIN s ON t1.a = s.b;

# Check that a plan can be found for this query. It used to fail with
# no plan found. We expect the left outer join to be performed as a hash
# join, as there are no indexes it could use in a nested loop join.
# The plan is affected by bug#36844820, and may give wrong results if
# the input data is changed (for example, wrong results are returned
# after UPDATE t SET c = 100). The plan is expected to change when that
# bug is fixed.
--replace_regex $elide_costs
--eval EXPLAIN FORMAT=TREE $query

--sorted_result
--eval $query

DROP TABLE t;

--echo #
--echo # Bug#37079336: Assert failure in
--echo # Item_eq_base::append_join_key_for_hash_join() with hypergraph
--echo #

CREATE TABLE small (id INT PRIMARY KEY, a INT, b TEXT);
INSERT INTO small VALUES (1, 1, 1), (2, 2, 2), (3, 3, 3), (4, 4, 4), (5, 5, 5);

CREATE TABLE big (a INT, b TEXT);
INSERT INTO big
  WITH RECURSIVE s(n) AS (
    SELECT 1 UNION ALL SELECT n+1 FROM s WHERE n < 100
  )
  SELECT n AS a, n AS b FROM s;

ANALYZE TABLE small, big;

# Used to return wrong results.
SELECT t1.b, t2.b, t3.b FROM
big AS t1,
LATERAL (SELECT * FROM big
         WHERE t1.a <> 100 ORDER BY a LIMIT 2) AS t2,
small AS t3
WHERE t1.a = t2.a AND t2.a = t3.id
ORDER BY t1.a;

# Used to hit an assertion failure.
SELECT t1.b, t2.b, t3.b
FROM small AS t1,
     LATERAL (SELECT DISTINCT * FROM big
              WHERE t1.a <> 100
              ORDER BY a LIMIT 2) AS t2,
     big AS t3 WHERE t1.a = t2.a AND t2.a = t3.a
ORDER BY t1.a;

DROP TABLE small, big;

--echo #
--echo # Bug#37746132 Result mismatch seen with Old Optimizer and Hypergraph
--echo #

CREATE TABLE t1 (
  pk INT PRIMARY KEY,
  a INT,
  b INT
);

INSERT INTO t1 VALUES (1,1,1), (2,2,2);
ANALYZE TABLE t1;

--echo # These two queries should give the same result.
SELECT COUNT(*) FROM t1 x4 WHERE EXISTS
(SELECT 1 FROM (SELECT x1.* FROM t1 x1 LEFT JOIN
 t1 x2 ON x1.a=x2.a AND x2.b > -x4.a) d1 STRAIGHT_JOIN t1 x3 ON d1.b < x3.b);

SELECT COUNT(*) FROM t1 x4 WHERE EXISTS
(SELECT 1 FROM (SELECT x1.* FROM t1 x1 LEFT JOIN
 t1 x2 ON x1.a=x2.a AND x2.b > -x4.a) d1 JOIN t1 x3 ON d1.b < x3.b);

DROP TABLE t1;

--echo #
--echo # Bug#37826935 Assertion `semijoin_group.size() ==
--echo # CountOrderElements(path->sort().order)' failed.
--echo #

CREATE TABLE t1 (pk int NOT NULL PRIMARY KEY, i1 int);

--echo # Should not trigger assert.
SELECT x2.i1 FROM t1 AS x1 , t1 AS x2
  WHERE ((x2.pk , x2.i1) IN
    (SELECT y1.i1, y1.pk FROM
      (t1 AS y1 JOIN t1 AS y2 ON y2.i1 = y1.pk AND y2.pk = y1.i1)))
  GROUP BY x2.i1;

DROP TABLE t1;

--echo #
--echo # Bug#36997713: Hypergraph : Result set mismatch (1 row vs empty set)
--echo #

CREATE TABLE t1 (f1 INTEGER, f2 VARCHAR(10));
CREATE TABLE t2 (f1 INTEGER, f2 VARCHAR(10));
INSERT INTO t1 VALUES (10, 'o');
INSERT INTO t2 VALUES (100, 'c');
INSERT INTO t2 VALUES (100, 'o');
ANALYZE TABLE t1,t2;

let $query =
SELECT *
FROM ((SELECT * FROM t1
       WHERE EXISTS (SELECT 1 FROM t2 WHERE t2.f2 >= t1.f2)) AS table1
INNER JOIN t2 AS table2 ON (table2.f2 = table1.f2))
WHERE (table2.f2 = 'x' ) OR (table1.f1 > 1);
--replace_regex $elide_costs
--eval EXPLAIN FORMAT=TREE $query
--eval $query

DROP TABLE t1,t2;

--echo #
--echo # Bug#37378072: `m_bitsets[i].is_inline()' in in at
--echo #               join_optimizer/overflow_bitset.h
--echo #
CREATE TABLE t1 (col1 INT, col2 INT);
CREATE TABLE t2 (pk INT PRIMARY KEY, txt TEXT, col1 INT, col2 INT, col3 INT,
col4 INT, col5 INT, col6 INT, col7 INT, col8 INT, col9 INT, col10 INT,
col11 INT, col12 INT, col13 INT, col14 INT, col15 INT, col16 INT, col17 INT,
col18 INT, col19 INT, col20 INT, col21 INT, col22 INT, col23 INT, col24 INT,
col25 INT, col26 INT, col27 INT, col28 INT, col29 INT, col30 INT, col31 INT,
col32 INT, col33 INT, col34 INT, col35 INT, col36 INT, col37 INT, col38 INT,
col39 INT, col40 INT, col41 INT, col42 INT, col43 INT, col44 INT, col45 INT,
col46 INT, col47 INT, col48 INT, col49 INT, col50 INT, col51 INT, col52 INT,
col53 INT, col54 INT, col55 INT, col56 INT, col57 INT, col58 INT, col59 INT,
col60 INT, col61 INT, col62 INT, col63 INT, col64 INT);
SELECT t2.pk FROM t1 JOIN t2 NATURAL JOIN t2 AS t3 ON t1.col1 = t3.col1 AND t1.col2 <> t3.col2;
DROP TABLE t1, t2;

--echo #
--echo # Bug#36630355: Assertion `false && "Inconsistent row counts for
--echo #               different AccessPath objects."' failed.
--echo #

CREATE TABLE t(a INT,
               b INT,
               c INT,
               d BIGINT,
               KEY (a, d));

SELECT 1
FROM t AS t1 STRAIGHT_JOIN
     t AS t2,
     t AS t3,
     t AS t4
WHERE t1.d = t3.d AND
      t3.a = t4.b AND
      t3.d = t4.c;

SELECT 1
FROM t AS t2 STRAIGHT_JOIN
     t AS t1,
     t AS t3,
     t AS t4
WHERE t1.d = t3.d AND
      t3.a = t4.b AND
      t3.d = t4.c;


DROP TABLE t;

--echo #
--echo # Bug#37452558 Assertion `path->num_output_rows() <=
--echo #         path->num_output_rows_before_filter` failed.
--echo #
CREATE TABLE t1 (f1 int, f2 time, KEY f1 (f1), KEY f2 (f2));
CREATE TABLE t2 (f1 int, f2 time, KEY f2 (f2));
INSERT INTO t1 VALUES
    (NULL,'00:20:03'),(2,'00:20:08'),(0,'00:20:01'),(NULL,'00:20:04'),
    (0,'00:20:01'),(NULL,'00:20:01'),(2,'00:20:04'),(NULL,'00:20:01'),
    (0,'00:20:06'),(NULL,'00:20:05'),(2,'00:20:09'),(NULL,'00:20:08'),
    (2,'00:20:03'),(1,'00:20:03'),(1,'00:20:03'),(NULL,'00:20:00'),
    (1,'00:20:01'),(NULL,'00:20:02'),(NULL,'00:20:06'),(0,'00:20:07'),
    (2,'00:20:07'),(0,'00:20:07'),(2,'00:20:03');
INSERT INTO t2 VALUES
    (0,'05:23:30'),(0,'22:10:55'),(0,'00:20:04'),(0,'00:20:08'),
    (0,'00:20:06'),(0,'00:20:07'),(0,'00:20:08'),(0,'00:20:06'),
    (0,'00:20:09');
ANALYZE TABLE t1,t2;
SELECT DISTINCT f1 FROM t1 WHERE f1 IN
    ( SELECT t2.f1 FROM t2 LEFT JOIN t1 USING (f2)) AND f1 IS NULL;
DROP TABLE t1,t2;

--echo #
--echo # Bug#37412816 `path->has_group_skip_scan' in (anonymous namespace)::
--echo #                SetGroupSkipScanCardinality
--echo #
CREATE TABLE t1 (f1 INTEGER, PRIMARY KEY (f1))  ;
CREATE TABLE t2 (f1 INTEGER, f2 DATETIME, KEY (f2));
INSERT INTO t1 VALUES (1), (2), (3), (4),(5),(6),(7),(8),(9),(10),
                      (11),(12),(13),(14),(15),(16),(17),(18),(19),(20);
ANALYZE TABLE t1, t2;
SELECT f1 FROM t1 WHERE (f1,f1) IN
    (SELECT f1,f2 FROM t2 WHERE f1 >= 1) GROUP BY f1;
DROP TABLE t1,t2;

--echo #
--echo # Bug#37667970: Hypergraph assertion failure - 'false && Inconsistent
--echo #               row counts for different AccessPath objects'
--echo #

CREATE TABLE t (a INT, b INT, c INT, KEY (c));
INSERT INTO t VALUES (1, 1, 1);

# An assertion failure was seen in debug builds. In release builds, a
# join plan was chosen. Now we expect the entire join to be optimized
# away, so that the plan is simply "Zero rows". It is known to return
# zero rows because ON FALSE makes the join return only
# NULL-complemented rows, and the WHERE clause rejects all the
# NULL-complemented rows.
EXPLAIN FORMAT=TREE
SELECT 1
FROM t AS t1 JOIN t AS t2 ON t1.c = t2.b AND t1.c = t2.c
     LEFT OUTER JOIN t AS t3 ON FALSE
WHERE t3.a = t2.a OR t2.b > t2.b;

DROP TABLE t;

--echo #
--echo # Bug#37733272: Planning overhead for ordered range queries in hypergraph
--echo #

CREATE TABLE t (
  pk INT PRIMARY KEY,
  a INT NOT NULL,
  b INT NOT NULL,
  c INT NOT NULL,
  UNIQUE KEY k1(a),
  KEY k2(b)
);
INSERT INTO t VALUES (1,1,1,1), (2,2,2,2), (3,3,3,3);
ANALYZE TABLE t;

SET optimizer_trace='enabled=on';
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT a, b FROM t ORDER BY a, b;
SET optimizer_trace='enabled=off';

# We expect only a single functional dependency, namely {a}->b, which
# helps us eliminate b from ORDER BY. Other functional dependencies
# from the unique indexes, such as {pk}->a, {pk}->b and {b}->c, do not
# help optimizing the query, so we don't collect them.
SELECT REGEXP_SUBSTR(TRACE, '(?s)Functional dependencies.*?""') AS dependencies
FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;

DROP TABLE t;

--echo #
--echo # Bug#38021487: Assertion `false && "Inconsistent row counts for
--echo #               different AccessPath objects."' failed.
--echo #

CREATE TABLE t (pk INT PRIMARY KEY, f1 INT, f2 INT);
INSERT INTO t VALUES (1, 1, 1), (2, 2, 2);
SELECT 1
FROM t AS t1
     JOIN t AS t2 ON t1.f1 = t2.f1 AND t2.f1 = t2.f2
     LEFT JOIN t AS t3 ON FALSE
WHERE t3.pk = t2.pk OR t2.f1 <> t2.f1;
DROP TABLE t;

--echo #
--echo # Bug#38418831: Hypergraph: Wrong result when sorting hash join
--echo #               result reading from temptable
--echo #

CREATE TABLE t1 (x INT);
INSERT INTO t1 VALUES (1), (3), (5), (7);

CREATE TABLE t2 (x INT, y LONGTEXT);
INSERT INTO t2 VALUES
  (1, REPEAT('This is the first blob.', 4)),
  (2, REPEAT('This is the second blob.', 4)),
  (3, REPEAT('This is the third blob.', 4)),
  (4, REPEAT('This is the fourth blob.', 4)),
  (5, REPEAT('This is the fifth blob.', 4)),
  (6, REPEAT('This is the sixth blob.', 4)),
  (7, REPEAT('This is the seventh blob.', 4)),
  (8, REPEAT('This is the eighth blob.', 4)),
  (9, REPEAT('This is the ninth blob.', 4)),
  (10, REPEAT('This is the tenth blob.', 4));

# The bug is seen when the temporary table is on disk. Reduce
# tmp_table_size to make it go to disk with a smaller test case.
SET tmp_table_size = 1024;

# Used to return 1 row instead of 4 rows.
SELECT /*+ NO_MERGE(t2) */ t1.x, t2.y
FROM t1 JOIN (SELECT * FROM t2) t2 ON t1.x = t2.x
ORDER BY t1.x, t2.x;

SET tmp_table_size = DEFAULT;
DROP TABLE t1, t2;

--echo #
--echo # Bug#38439704: SIG6 with hypergraph optimizer - assertion failure in
--echo #               RedundantThroughSargable at join_optimizer.cc
--echo #

CREATE TABLE t(
  pk INT PRIMARY KEY, col1 INT, col2 INT, col3 INT, col4 INT, col5 INT,
  col6 INT, col7 INT, col8 INT, col9 INT, col10 INT, col11 INT, col12 INT,
  col13 INT, col14 INT, col15 INT, col16 INT, col17 INT, col18 INT, col19 INT,
  col20 INT, col21 INT, col22 INT, col23 INT, col24 INT, col25 INT, col26 INT,
  col27 INT, col28 INT, col29 INT, col30 INT, col31 INT, col32 INT, col33 INT,
  col34 INT, col35 INT, col36 INT, col37 INT, col38 INT, col39 INT, col40 INT,
  col41 INT, col42 INT, col43 INT, col44 INT, col45 INT, col46 INT, col47 INT,
  col48 INT, col49 INT, col50 INT, col51 INT, col52 INT, col53 INT, col54 INT,
  col55 INT, col56 INT, col57 INT, col58 INT, col59 INT, col60 INT, col61 INT,
  col62 INT, col63 INT, col64 INT
);

INSERT INTO t(pk) VALUES (1), (2);

SELECT 1
FROM t AS t1
     NATURAL JOIN t AS t2
     JOIN t AS t3 ON t2.pk = t3.pk
WHERE t1.col1 <> t3.col1 AND t1.col2 <> t3.col2;

DROP TABLE t;

--echo #
--echo # Bug#38440648: Assertion `false && "Inconsistent row counts for
--echo #               different AccessPath objects."' failed.
--echo #

CREATE TABLE t1 (col_int INT);
CREATE TABLE t2 (col_int INT, col_varchar10 VARCHAR(10));
CREATE TABLE t3 (pk INT PRIMARY KEY, col_varchar20 VARCHAR(20));

SELECT 1 FROM t1, t2, t3
WHERE t2.col_int = t3.pk AND
      t2.col_varchar10 = t3.col_varchar20 AND
      t1.col_int = t3.pk AND
      t1.col_int >= 8;

DROP TABLE t1, t2, t3;

--echo #
--echo # Bug#38557567: Assertion `IsEmpty(child.delayed_predicates)'
--echo #               failed|join_optimizer/cost_model.cc
--echo #

CREATE TABLE t(i INT PRIMARY KEY);

SELECT LAG (i) OVER () FROM t AS t1
WHERE i IN (SELECT i FROM t AS t2)
GROUP BY i;

DROP TABLE t;

--echo #
--echo # Bug#38704535: Assertion `std::cmp_less(pred_idx,
--echo #               m_graph->num_filter_predicates)' failed.
--echo #

CREATE TABLE t(a INT, KEY(a));
INSERT INTO t VALUES (1), (2), (3);
ANALYZE TABLE t;

let $query =
    SELECT *
    FROM
       t AS t1 LEFT JOIN
       t AS t2 INNER JOIN
       (SELECT DISTINCT a FROM t WHERE a <> a) AS t3
       ON t3.a = t2.a
       ON TRUE
    WHERE t3.a = 1 OR t1.a = 2;

# The inner join between t2 and t3 should be pruned and replaced by "Zero rows".
# This query used to trigger an assert failure.
--replace_regex $elide_costs
--eval EXPLAIN FORMAT=TREE $query
--sorted_result
--eval $query

# Also check that the entire query is optimized away when the empty
# derived table is inner-joined to the outer tables.
--let $query = `SELECT REPLACE('$query', 'LEFT JOIN', 'INNER JOIN')`
--eval EXPLAIN FORMAT=TREE $query

DROP TABLE t;

--echo #
--echo # Bug#38763106 - sort with limit in hypergraph gets wrong result
--echo #

CREATE TABLE t1 (char_0_col CHAR(0) COLLATE utf8mb4_general_ci NOT NULL);
INSERT INTO t1 VALUES (''), ('');
ANALYZE TABLE t1;

--replace_regex $elide_costs
EXPLAIN FORMAT=TREE
SELECT DISTINCT char_0_col FROM t1;

--replace_regex $elide_costs
EXPLAIN FORMAT=TREE
SELECT char_0_col FROM t1
ORDER BY char_0_col LIMIT 2;

--echo # Test 1: Basic LIMIT 1 (original bug case)
SELECT * FROM t1 ORDER BY char_0_col LIMIT 1;

--echo # Test 2: LIMIT equals number of rows
SELECT * FROM t1 ORDER BY char_0_col LIMIT 2;

--echo # Test 3: LIMIT greater than number of rows
SELECT * FROM t1 ORDER BY char_0_col LIMIT 5;

--echo # Test 4: LIMIT with OFFSET 0 (should behave like LIMIT 1)
SELECT * FROM t1 ORDER BY char_0_col LIMIT 1 OFFSET 0;

--echo # Test 5: LIMIT with OFFSET 1 (skip first row)
SELECT * FROM t1 ORDER BY char_0_col LIMIT 1 OFFSET 1;

--echo # Test 6: LIMIT with OFFSET equal to total rows (no rows)
SELECT * FROM t1 ORDER BY char_0_col LIMIT 1 OFFSET 2;

--echo # Test 7: LIMIT with OFFSET greater than total rows (no rows)
SELECT * FROM t1 ORDER BY char_0_col LIMIT 1 OFFSET 5;

--echo # Test 8: Multiple rows with various LIMIT values
INSERT INTO t1 VALUES (''), (''), ('');

SELECT * FROM t1 ORDER BY char_0_col LIMIT 1;
SELECT * FROM t1 ORDER BY char_0_col LIMIT 3;
SELECT * FROM t1 ORDER BY char_0_col LIMIT 10;

--echo # Test 9: LIMIT with OFFSET on larger dataset
SELECT * FROM t1 ORDER BY char_0_col LIMIT 2 OFFSET 1;
SELECT * FROM t1 ORDER BY char_0_col LIMIT 2 OFFSET 3;

--echo # Test 10: Verify with COUNT to ensure correct row counts
SELECT COUNT(*) FROM (
  SELECT * FROM t1 ORDER BY char_0_col LIMIT 1
) AS derived;

SELECT COUNT(*) FROM (
  SELECT * FROM t1 ORDER BY char_0_col LIMIT 3
) AS derived;

SELECT COUNT(*) FROM (
  SELECT * FROM t1 ORDER BY char_0_col LIMIT 1 OFFSET 2
) AS derived;

--echo # Test 11: DESC order with LIMIT
SELECT * FROM t1 ORDER BY char_0_col DESC LIMIT 1;
SELECT * FROM t1 ORDER BY char_0_col DESC LIMIT 2 OFFSET 1;

--echo # Test 12: Explicit hypergraph hint (ensure it works)
SELECT /*+ SET_VAR(optimizer_switch="hypergraph_optimizer=on") */
* FROM t1 ORDER BY char_0_col LIMIT 1;

--echo # Test 13: Verify old optimizer still works (regression check)
SELECT /*+ SET_VAR(optimizer_switch="hypergraph_optimizer=off") */
* FROM t1 ORDER BY char_0_col LIMIT 1;

--echo # Test 14: LIMIT 0 (edge case - should return no rows)
SELECT * FROM t1 ORDER BY char_0_col LIMIT 0;

--echo # Test 15: Large OFFSET that skips all rows
SELECT * FROM t1 ORDER BY char_0_col LIMIT 1 OFFSET 100;

--echo # Test 16: DISTINCT with zero-length key should yield 1 row
SELECT DISTINCT char_0_col FROM t1 ORDER BY char_0_col;

--echo # Test 17: DISTINCT with LIMIT should still yield at most 1
SELECT DISTINCT char_0_col FROM t1 ORDER BY char_0_col LIMIT 5;

--echo # Test 18: DISTINCT with hypergraph hint
SELECT /*+ SET_VAR(optimizer_switch="hypergraph_optimizer=on") */
DISTINCT char_0_col FROM t1 ORDER BY char_0_col;

DROP TABLE t1;

--echo #
--echo # Bug#38292966: Hypergraph: Optimizer trace should not state
--echo #               "This is a bug" for known inconsistent row counts
--echo #

CREATE TABLE num (n INT);
INSERT INTO num VALUES (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
CREATE TABLE t1 (a INT, b INT);
INSERT INTO t1 SELECT n,n FROM num UNION SELECT n+10,n+10 FROM num;
CREATE TABLE t2 (a INT, b INT);
ANALYZE TABLE t1, t2;

SET optimizer_trace='enabled=on';
EXPLAIN FORMAT=TREE SELECT SQL_BIG_RESULT x1.a+0 k, COUNT(x1.b) FROM t1 x1
  LEFT JOIN t2 x2 ON x1.b=x2.a
  LEFT JOIN t1 x3 ON x2.b=x3.a GROUP BY k;

# The optimizer trace should contain a WARNING about inconsistent row counts
# but must NOT contain the alarming "This is a bug" language.
--let $trace = `SELECT TRACE FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE`

SET optimizer_trace='enabled=off';

--let $has_warning = `SELECT LOCATE('has inconsistent row counts with', '$trace') > 0`
--let $has_bug_msg = `SELECT LOCATE('This is a bug', '$trace') > 0`

if ($has_warning)
{
  --echo Optimizer trace correctly contains WARNING about inconsistent row counts.
}
if (!$has_warning)
{
  --echo Optimizer trace does NOT contain expected WARNING about inconsistent row counts.
}
if ($has_bug_msg)
{
  --echo FAIL: Optimizer trace still contains "This is a bug" message.
}
if (!$has_bug_msg)
{
  --echo Optimizer trace correctly omits "This is a bug" message.
}

DROP TABLE num, t1, t2;

--echo #
--echo # Bug#38822359: Assertion `std::cmp_less(pred_idx,
--echo #               m_graph->num_where_predicates)' failed.
--echo #
CREATE TABLE t(f1 INT PRIMARY KEY, f2 INT, KEY(f2));
INSERT INTO t VALUES (1, 1), (2, 2), (3, 3);
ANALYZE TABLE t;

let $query =
    SELECT t1.f1 FROM t AS t1 LEFT JOIN (
      (SELECT DISTINCT * FROM t AS t2 WHERE f2 <> f2) AS dt1,
      (SELECT t4.* FROM t AS t3 STRAIGHT_JOIN t AS t4 ON t4.f1 = t3.f1) AS dt2
    ) ON TRUE;

--echo # Verify that the assertion does not fail, and that t2, t3 and t4 are
--echo # pruned from the query plan and replaced by "Zero rows".
--replace_regex $elide_costs
--eval EXPLAIN FORMAT=TREE $query
--sorted_result
--eval $query

DROP TABLE t;

--echo #
--echo # Bug#38585820: Result mismatch with GROUP BY on DATE column
--echo #

# This is a copy of the same test case from type_datetime.test.
# This test case can be deleted when hypergraph is enabled in optimized builds.

CREATE TABLE t1 (
    d DATE DEFAULT NULL,
    t TIME DEFAULT NULL,
    dt DATETIME DEFAULT NULL,
    pk INT NOT NULL PRIMARY KEY,
    ci INT DEFAULT NULL
);

INSERT INTO t1 VALUES
  ('1982-07-14','11:12:13','1982-07-14 11:12:13',1,-2086898178),
  (NULL,NULL,NULL,2,-549278089),
  ('2035-09-13','12:13:14','2035-09-13 12:13:14',3,1616004595),
  ('2011-03-24','13:14:15','2011-03-24 13:14:15',4,-1457068597),
  ('1976-05-25','14:15:16','1976-05-25 14:15:16',5,-726291594),
  ('1986-12-09','15:16:17','1986-12-09 15:16:17',6,1775995858),
  ('2017-05-17','16:17:18','2017-05-17 16:17:18',7,-1267712153),
  ('2034-09-09','17:18:19','2034-09-09 17:18:19',8,514287676),
  ('2033-06-22','18:19:20','2033-06-22 18:19:20',9,1526531818),
  (NULL,NULL,NULL,10,-1026887600);
let $date_query=
SELECT alias2.d AS field1,
       MIN(alias1.pk) AS field2
FROM t1 AS alias1 LEFT JOIN
       t1 AS alias2 RIGHT JOIN t1 AS alias3
       ON alias3.ci = alias2.pk
     ON alias3.ci = alias2.ci
GROUP BY field1;

let $time_query=
SELECT alias2.t AS field1,
       MIN(alias1.pk) AS field2
FROM t1 AS alias1 LEFT JOIN
       t1 AS alias2 RIGHT JOIN t1 AS alias3
       ON alias3.ci = alias2.pk
     ON alias3.ci = alias2.ci
GROUP BY field1;

let $datetime_query=
SELECT alias2.dt AS field1,
       MIN(alias1.pk) AS field2
FROM t1 AS alias1 LEFT JOIN
       t1 AS alias2 RIGHT JOIN t1 AS alias3
       ON alias3.ci = alias2.pk
     ON alias3.ci = alias2.ci
GROUP BY field1;

SET optimizer_switch='hypergraph_optimizer=off';

eval $date_query;
eval $time_query;
eval $datetime_query;

SET optimizer_switch='hypergraph_optimizer=on';

eval $date_query;
eval $time_query;
eval $datetime_query;

SET optimizer_switch='hypergraph_optimizer=default';

DROP TABLE t1;

--echo #
--echo # Bug#38866140: Inconsistent row counts assertion with RAND
--echo #              predicates in hypergraph optimizer
--echo #

CREATE TABLE t (i INT);
INSERT INTO t VALUES (1), (2), (3);
ANALYZE TABLE t;

CREATE TABLE t1 (
  col_date3 DATE DEFAULT NULL,
  col_int INT DEFAULT NULL,
  col_datetime6 DATETIME DEFAULT NULL,
  col_tinytext_character_set_utf8mb4_key TINYTEXT
);
INSERT INTO t1 VALUES
  ('2024-01-01', 1, '2024-01-01 00:00:00', 'abc'),
  ('2024-02-01', 2, '2024-02-01 00:00:00', 'def'),
  ('2024-03-01', 3, '2024-03-01 00:00:00', 'ghi');
ANALYZE TABLE t1;

# Test 1: Simplified reproducer.
# RAND() in a WHERE predicate between two tables (3-table inner join).
--disable_result_log
SELECT 1 FROM t AS t1, t AS t2, t AS t3
WHERE t1.i <> t2.i AND t2.i = t3.i + RAND() AND t1.i <> t3.i;
--enable_result_log

# Test 2: Original bug report query.
# RAND() in an ON clause of an outer join spanning multiple join nests.
--disable_result_log
SELECT table2.col_date3 AS field2
FROM t1 AS table1
RIGHT JOIN t1 AS table2
  LEFT JOIN t1 AS table3 ON table2.col_date3 <> table3.col_int
ON REPLACE(table1.col_date3, '', RAND(table3.col_int))
   LIKE table3.col_tinytext_character_set_utf8mb4_key
WHERE table1.col_tinytext_character_set_utf8mb4_key >
      table2.col_tinytext_character_set_utf8mb4_key;
--enable_result_log

# Test 3: RAND() in ON clause of explicit INNER JOIN.
--disable_result_log
SELECT 1 FROM t AS t1
INNER JOIN t AS t2 ON t1.i <> t2.i
INNER JOIN t AS t3 ON t2.i = t3.i + RAND()
WHERE t1.i <> t3.i;
--enable_result_log

# Test 4: RAND() as the ONLY predicate between a specific table pair.
--disable_result_log
SELECT 1 FROM t AS t1, t AS t2, t AS t3, t AS t4
WHERE t1.i <> t2.i
  AND t2.i <> t4.i
  AND t3.i <> t4.i
  AND t2.i = t3.i + RAND();
--enable_result_log

# Test 5: Multiple RAND predicates in the same query.
--disable_result_log
SELECT 1 FROM t AS t1, t AS t2, t AS t3
WHERE t1.i + RAND() = t2.i
  AND t2.i + RAND() = t3.i
  AND t1.i <> t3.i
  AND t1.i <> t2.i;
--enable_result_log

# Test 6: RAND() with 4 tables.
--disable_result_log
SELECT 1 FROM t AS t1, t AS t2, t AS t3, t AS t4
WHERE t1.i <> t2.i
  AND t2.i = t3.i + RAND()
  AND t3.i <> t4.i
  AND t1.i <> t4.i;
--enable_result_log

# Test 7: RAND() in LEFT JOIN ON clause with 3 tables.
--disable_result_log
SELECT * FROM t AS t1
INNER JOIN t AS t3 ON t1.i <> t3.i
LEFT JOIN t AS t2 ON t1.i = t2.i + RAND()
WHERE t2.i IS NULL OR t2.i <> t3.i;
--enable_result_log

# Test 8: RAND() combined with a deterministic predicate in WHERE.
--disable_result_log
SELECT 1 FROM t AS t1, t AS t2, t AS t3
WHERE t1.i = t2.i AND t2.i = t3.i + RAND() AND t1.i <> t3.i;
--enable_result_log

# Test 9: Non-RAND nondeterministic function (UUID_SHORT).
--disable_result_log
SELECT 1 FROM t AS t1, t AS t2, t AS t3
WHERE t1.i <> t2.i AND t2.i = t3.i + UUID_SHORT() AND t1.i <> t3.i;
--enable_result_log

DROP TABLE t, t1;

--echo #
--echo # Bug#38097990: Const folding caused inequivalent join_cond_optim
--echo #               in hypergraph
--echo #

CREATE TABLE t1 (
    id_col       INT PRIMARY KEY,
    smallint_col SMALLINT,
    int_col      INT
);

CREATE TABLE t2 (
    id_col       INT PRIMARY KEY,
    smallint_col SMALLINT,
    int_col      INT
);

# The ON condition contains (ref_0.id_col IS NOT NULL) which folds to TRUE
# because id_col is PRIMARY KEY (NOT NULL). This caused remove_eq_conds()
# to mutate the Item_cond_or argument list in-place, making
# tl->join_cond_optim() logically inequivalent to the original condition.
# The fix rebuilds join_cond_optim from the post-folding conditions.
SELECT COUNT(ref_1.int_col)
FROM   t1 AS ref_0
       LEFT JOIN t2 AS ref_1
              ON ( ( ref_0.smallint_col IS NULL )
                    OR ( ref_0.id_col IS NOT NULL ) );

# Verify with data: insert rows and confirm the LEFT JOIN ON TRUE
# produces correct results.
INSERT INTO t1 VALUES (1, 10, 100), (2, NULL, 200);
INSERT INTO t2 VALUES (1, 10, 100);
ANALYZE TABLE t1, t2;

SELECT COUNT(ref_1.int_col)
FROM   t1 AS ref_0
       LEFT JOIN t2 AS ref_1
              ON ( ( ref_0.smallint_col IS NULL )
                    OR ( ref_0.id_col IS NOT NULL ) );

# Also test AND with constant-foldable condition.
SELECT COUNT(ref_1.int_col)
FROM   t1 AS ref_0
       LEFT JOIN t2 AS ref_1
              ON ( ( ref_0.smallint_col IS NULL )
                    AND ( ref_0.id_col IS NOT NULL ) );

DROP TABLE t1, t2;

--echo #
--echo # Bug#38867038	Assertion `nullptr != dynamic_cast<Target>(arg)' failed.
--echo #

CREATE TABLE A(col_int INT);
INSERT INTO A VALUES (1), (2), (3), (4), (5);

--sorted_result
SELECT  /*+ SET_VAR(optimizer_switch='hypergraph_optimizer=on') */
PERCENT_RANK() OVER ( ORDER BY SEC_TO_TIME(3600) RANGE INTERVAL 7 QUARTER PRECEDING )
FROM A;

DROP TABLE A;

--echo #
--echo # Bug#38473156: hypergraph optimizer errors out on an
--echo #               otherwise valid query the mysql optimizer
--echo #               executes just fine.
--echo #

CREATE TABLE t1 (
  id INT UNSIGNED NOT NULL AUTO_INCREMENT,
  col1 INT UNSIGNED NOT NULL,
  col2 INT UNSIGNED NOT NULL,
  data JSON DEFAULT NULL,
  PRIMARY KEY (id),
  KEY idx_col1 (col1),
  KEY idx_col2 (col2)
);

INSERT INTO t1 (col1, col2, data)
  VALUES (111, 222, JSON_OBJECT('data', REPEAT('LotsOfData', 250000)));

let $query = SELECT * FROM t1 ORDER BY id;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
set optimizer_trace="enabled=on";
--disable_result_log
eval $query;
--enable_result_log

WITH trace_vals AS (
  SELECT REGEXP_SUBSTR(trace, 'INDEX_SCAN, cost=[0-9.]+', 1, 1) AS idx_line,
         REGEXP_SUBSTR(trace, 'TABLE_SCAN, cost=[0-9.]+', 1, 1) AS tbl_line
    FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE
) SELECT (tbl_line IS NULL OR
         (CAST(SUBSTRING_INDEX(idx_line, 'cost=', -1) AS DECIMAL(20,10)) =
          CAST(SUBSTRING_INDEX(tbl_line, 'cost=', -1) AS DECIMAL(20,10))))
         AS costs_equal
  FROM trace_vals;

set optimizer_trace="enabled=off";

DROP TABLE t1;

--echo #
--echo # Bug#38389762: Hypergraph: Should not propose both table
--echo #              scan and primary index scan
--echo #

CREATE TABLE t1 ( f1 INT PRIMARY KEY, f2 CHAR(5));
INSERT INTO t1 VALUES (1,'aaaaa'), (2,'bbbbb'), (3,'ccccc'), (4,'ddddd');
ANALYZE TABLE t1;

--echo # Clustered primary key should avoid proposing table scan.
SET optimizer_trace='enabled=on';
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1;
SELECT LOCATE('{TABLE_SCAN, cost=', trace) = 0 AS no_table_scan
  FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;

DROP TABLE t1;

CREATE TABLE t1 ( f1 INT, f2 INT, f3 INT, PRIMARY KEY(f1,f2));
INSERT INTO t1 VALUES (1,2,1), (1,3,1), (3,1,2), (3,2,1);
ANALYZE TABLE t1;

--echo # Index lookup on clustered primary key should avoid proposing table scan.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t1 WHERE f1 = 3;
SELECT LOCATE('{TABLE_SCAN, cost=', trace) = 0 AS no_table_scan
  FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
SET optimizer_trace='enabled=off';

DROP TABLE t1;

--echo #
--echo # Bug#35368475: FilterIterator should short-circuit on constant-false
--echo #               conditions
--echo #
--echo # When the filter condition is const_for_execution() and evaluates to
--echo # false, FilterIterator should return EOF immediately without scanning
--echo # the source iterator.
--echo #

CREATE TABLE t(x INT);
INSERT INTO t VALUES (), (), (), (), (), (), (), (), (), ();
ANALYZE TABLE t;

--echo # Test 1: Constant-false uncorrelated subquery.
--echo # "never executed" on table scan proves DoInit() short-circuited.
--replace_regex /actual time=\S+/actual time=N/g /cost=\S+/cost=N/g
EXPLAIN ANALYZE SELECT 1 FROM t t1 WHERE 100 = (SELECT COUNT(*) FROM t t2);
SELECT 1 FROM t t1 WHERE 100 = (SELECT COUNT(*) FROM t t2);

--echo # Test 2: Constant-true subquery -- all 10 rows returned normally.
SELECT 1 FROM t t1 WHERE 10 = (SELECT COUNT(*) FROM t t2);

--echo # Test 3: NULL subquery condition -- 0 rows (NULL is not true).
SELECT 1 FROM t t1 WHERE NULL = (SELECT COUNT(*) FROM t t2);

--echo # Test 4: Mixed conjunction -- constant-false subquery short-circuits.
--echo # "never executed" proves source was never initialized.
--replace_regex /actual time=\S+/actual time=N/g /cost=\S+/cost=N/g
EXPLAIN ANALYZE SELECT 1 FROM t t1
  WHERE 0 = (SELECT COUNT(*) FROM t t2) AND t1.x IS NULL;
SELECT 1 FROM t t1
  WHERE 0 = (SELECT COUNT(*) FROM t t2) AND t1.x IS NULL;

--echo # Test 5: Mixed conjunction, constant-true -- no short-circuit.
SELECT 1 FROM t t1
  WHERE 10 = (SELECT COUNT(*) FROM t t2) AND t1.x IS NULL;

--echo # Test 6: Constant-false as the second AND-term.
--replace_regex /actual time=\S+/actual time=N/g /cost=\S+/cost=N/g
EXPLAIN ANALYZE SELECT 1 FROM t t1
  WHERE t1.x IS NULL AND 0 = (SELECT COUNT(*) FROM t t2);

--echo # Test 7: NULL conjunct in mixed AND -- short-circuits.
SELECT 1 FROM t t1
  WHERE NULL = (SELECT COUNT(*) FROM t t2) AND t1.x IS NULL;

--echo # Test 8: Three-way AND with one constant-false term.
SELECT 1 FROM t t1
  WHERE t1.x IS NULL AND 0 = (SELECT COUNT(*) FROM t t2) AND t1.x IS NOT NULL;

--echo # Test 9: Three-way AND, all constant-true -- returns rows normally.
SELECT 1 FROM t t1
  WHERE 10 = (SELECT COUNT(*) FROM t t2) AND 1 = 1 AND t1.x IS NULL;

DROP TABLE t;

--echo #
--echo # Bug#37173479: Hypergraph doesn't elide sort on effectively constant
--echo #               expressions
--echo #
--echo # When WHERE pins all PK columns to constant values, all columns FD on
--echo # the PK are also constant. ORDER BY on such columns should not require
--echo # a sort.
--echo #

CREATE TABLE t(id INT PRIMARY KEY, a INT);
INSERT INTO t VALUES (1, 10), (2, 20), (3, 30);
ANALYZE TABLE t;

--echo # Test 1: Single PK constant => ORDER BY dependent column elides sort
let $query = SELECT * FROM t WHERE id = 1 ORDER BY a;
eval EXPLAIN FORMAT=TREE $query;
eval $query;

--echo # Control: Without WHERE, sort IS needed
EXPLAIN FORMAT=TREE SELECT * FROM t ORDER BY a;

DROP TABLE t;

--echo # Test 2: Multi-column PK, all constant => sort elided
CREATE TABLE t2(id1 INT, id2 INT, a INT, PRIMARY KEY(id1, id2));
INSERT INTO t2 VALUES (1, 1, 100), (1, 2, 200), (2, 1, 300);
ANALYZE TABLE t2;

let $query = SELECT * FROM t2 WHERE id1 = 1 AND id2 = 2 ORDER BY a;
eval EXPLAIN FORMAT=TREE $query;
eval $query;

DROP TABLE t2;

--echo # Test 3: Multiple ORDER BY columns, all FD-dependent on constant PK
CREATE TABLE t3(id INT PRIMARY KEY, a INT, b INT);
INSERT INTO t3 VALUES (1, 10, 100), (2, 20, 200), (3, 30, 300);
ANALYZE TABLE t3;

let $query = SELECT * FROM t3 WHERE id = 1 ORDER BY a, b;
eval EXPLAIN FORMAT=TREE $query;
eval $query;

DROP TABLE t3;

--echo # Test 4: Partial PK constant - sort must NOT be elided (negative test)
CREATE TABLE t4(id1 INT, id2 INT, a INT, PRIMARY KEY(id1, id2));
INSERT INTO t4 VALUES (1, 1, 100), (1, 2, 200), (2, 1, 300);
ANALYZE TABLE t4;

let $query = SELECT * FROM t4 WHERE id1 = 1 ORDER BY a;
eval EXPLAIN FORMAT=TREE $query;
eval $query;

DROP TABLE t4;

--echo # Test 5: NOT NULL unique key constant => sort elided
CREATE TABLE t5(id INT PRIMARY KEY, a INT NOT NULL, b INT, UNIQUE KEY uk_a(a));
INSERT INTO t5 VALUES (1, 10, 100), (2, 20, 200), (3, 30, 300);
ANALYZE TABLE t5;

let $query = SELECT * FROM t5 WHERE a = 10 ORDER BY b;
eval EXPLAIN FORMAT=TREE $query;
eval $query;

DROP TABLE t5;

--echo #
--echo # Bug#39062785: Assertion `tl->join_cond_optim() != nullptr'
--echo #               failed.
--echo #

CREATE TABLE t1 (f1 INTEGER, f2 INTEGER, KEY (f2));
let $query = SELECT 1 FROM t1
             WHERE (t1.f2 < ANY (SELECT t2.f1 FROM t1 AS t2
                                 WHERE t2.f1 = t1.f1 AND
                                       NOT EXISTS (SELECT t2.f1
                                                   FROM t1 AS t3 )));
--replace_regex $elide_costs
eval EXPLAIN FORMAT=TREE $query;
eval $query;
DROP TABLE t1;

--source include/disable_hypergraph.inc
