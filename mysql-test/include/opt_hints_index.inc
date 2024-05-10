--source include/elide_costs.inc

CREATE TABLE t1 (a INT, b INT, c INT, d INT,
                 KEY i_a(a), KEY i_b(b),
                 KEY i_ab(a,b), KEY i_c(c), KEY i_d(d));
INSERT INTO t1 VALUES
(1,1,1,1),(2,2,2,1),(3,3,3,1),(4,4,4,1),
(5,5,5,1),(6,6,6,1),(7,7,7,1),(8,8,8,1);
INSERT INTO t1 SELECT a,b, c + 10, d FROM t1;
INSERT INTO t1 SELECT a,b, c + 20, d FROM t1;
INSERT INTO t1 SELECT a,b, c + 40, d FROM t1;
INSERT INTO t1 SELECT a,b, c + 80, d FROM t1;
INSERT INTO t1 SELECT a,b, c + 160, d FROM t1;
ANALYZE TABLE t1;

SET EXPLAIN_FORMAT=tree;
# Check behavior of duplicated/intersected hints.
# First specified hint is applied and next conflicting/intersected hints
# are ignored with warning.

# JOIN_INDEX(t1) is ignored as intersected.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX(t1) JOIN_INDEX(t1) */ a FROM t1;

# INDEX(t1 i_d) is ignored as duplicated.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX(t1 i_a, i_b, i_c) INDEX(t1 i_d) */ a FROM t1;

# JOIN_INDEX(t1 i_a, i_b) is ignored as intersected.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX(t1 i_a) JOIN_INDEX(t1 i_a, i_b) JOIN_INDEX(t1 i_b) */ a FROM t1;

# GROUP_INDEX(t1 i_a, i_b) is ignored as intersected.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX(t1 i_a) GROUP_INDEX(t1 i_a, i_b) GROUP_INDEX(t1 i_b) */ a FROM t1;

# GROUP_INDEX(t1) is ignored as intersected.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX(t1 i_a) GROUP_INDEX(t1) */ a FROM t1;

# ORDER_INDEX(t1 i_a, i_b) is ignored as intersected.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX(t1 i_a) ORDER_INDEX(t1 i_a, i_b) ORDER_INDEX(t1 i_b) */ a FROM t1;

# ORDER_INDEX(t1) is ignored as intersected.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX(t1 i_a) ORDER_INDEX(t1) */ a FROM t1;

# ORDER_INDEX(t1 i_b) is ignored as intersected.
--replace_regex $elide_costs
EXPLAIN SELECT /*+  ORDER_INDEX(t1 i_a, i_b) ORDER_INDEX(t1 i_b) INDEX(t1 i_c)*/ a FROM t1;

# INDEX(t1) is ignored as intersected.
--replace_regex $elide_costs
EXPLAIN SELECT /*+  ORDER_INDEX(t1 i_a, i_b) GROUP_INDEX(t1 i_b) INDEX(t1)*/ a FROM t1;

# INDEX(t1) is ignored as intersected.
--replace_regex $elide_costs
EXPLAIN SELECT /*+  ORDER_INDEX(t1) GROUP_INDEX(t1) INDEX(t1)*/ a FROM t1;

# Check the use of index hints.

# Force the use of i_a, i_b indexes, intersect(i_a,i_b) is used.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX(t1 i_a, i_b) */ a FROM t1 WHERE a = 1 AND b = 2 AND c = 3;

# Force the use of i_a, i_ab indexes, i_ab index is used.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX(t1 i_a, i_ab) */ a FROM t1 WHERE a = 1 AND b = 2 AND c = 3;

#Force the use of i_a, i_b, i_c indexes, i_c index is used.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX(t1 i_a, i_b) JOIN_INDEX(t1 i_c) */ * FROM t1 WHERE a = 1 AND b = 2 AND c = 3;

# Ignore i_ab index, i_b index is used.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ NO_INDEX(t1 i_ab) */ a, max(b) FROM t1 WHERE b = 2 GROUP BY a;

# Ignore i_ab index, i_b index is used.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ NO_JOIN_INDEX(t1 i_ab) */ a, max(b) FROM t1 WHERE b = 2 GROUP BY a;

# Force i_ab index for GROUP BY, i_ab index scan is used.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ GROUP_INDEX(t1 i_ab) */ a, max(b) FROM t1 GROUP BY a;

# Ignore using i_a to avoid sorting for GROUP BY. Expect temp-table aggregation.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ NO_GROUP_INDEX(t1 i_a) */ a, COUNT(*) FROM t1 GROUP BY a;

# Force i_ab index for JOIN, i_ab loose index scan is used.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ JOIN_INDEX(t1 i_ab) */ a, max(b) FROM t1 GROUP BY a;

# Ignore i_ab for for sorting rows. i_a index is used for sorting rows.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ NO_ORDER_INDEX(t1 i_ab) */ a FROM t1
ORDER BY a;

# Ignore i_a for for sorting rows. Filesort is used for sorting rows.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ NO_ORDER_INDEX(t1 i_a) */ a FROM t1
ORDER BY a;

# Force i_ab index for sorting rows.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ ORDER_INDEX(t1 i_ab) */ a FROM t1
ORDER BY a;

# Force i_a index for sorting rows.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ ORDER_INDEX(t1 i_a) */ a FROM t1
ORDER BY a;

# Force i_a index for sorting rows.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ ORDER_INDEX(t1 i_a) */ * FROM t1
ORDER BY a;

# i_a cannot be used for ref access.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ ORDER_INDEX(t1 i_a) */ a FROM t1 WHERE a = 100;

# Ignore all indexes.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ NO_INDEX(t1) */ * FROM t1 WHERE a = 1 AND b = 2 AND c = 3;

# Check if old hints work if no new hint is specified.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX_MERGE(t1) */ * FROM t1 IGNORE INDEX (i_a)
WHERE a = 1 AND b = 2 AND c = 3;

# Check index hints with UPDATE/DELETE commands.

# Force i_a index for UPDATE.
--replace_regex $elide_costs
EXPLAIN UPDATE /*+ INDEX(t1 i_a) */ t1 SET d = 1 WHERE a = 1 AND b = 2 AND c = 3;

# Force i_a index for DELETE.
--replace_regex $elide_costs
EXPLAIN DELETE /*+ INDEX(t1 i_a) */ FROM t1 WHERE a = 1 AND b = 2 AND c = 3;

# Check index hints in views.

CREATE VIEW v1 AS SELECT /*+ NO_INDEX(t1 i_a,i_b) */ a FROM t1 WHERE
 b IN (SELECT /*+ NO_INDEX(t1 i_ab,i_b) */ a FROM t1 WHERE a > 3)
ORDER BY a;
SHOW CREATE VIEW v1;
--replace_regex $elide_costs
EXPLAIN SELECT a FROM v1;
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX(ta i_a) */ ta.a FROM v1, t1 ta WHERE ta.a > 3;

CREATE VIEW v2 AS SELECT /*+ INDEX(ta i_a) */ ta.a FROM v1, t1 ta WHERE ta.a > 3;
SHOW CREATE VIEW v2;
--replace_regex $elide_costs
EXPLAIN SELECT a FROM v2;
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX(tb i_a) */ tb.a FROM v2, t1 tb WHERE tb.a > 3;

# Check that old hint is silently ignored if new hint is specified.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ INDEX(t1 i_a) */ * FROM t1 IGNORE INDEX(i_a)
WHERE a = 1 AND b = 2 AND c = 3;

# Check unsupported hints in view.
CREATE VIEW v3 AS SELECT /*+ INDEX_MERGE(t1) */a FROM t1
WHERE a = 1 AND b = 2 AND c = 3;

DROP VIEW v1, v2, v3;
DROP TABLE t1;

# Check hint support for full text indexes
CREATE TABLE t(
FTS_DOC_ID BIGINT UNSIGNED AUTO_INCREMENT NOT NULL PRIMARY KEY,
x VARCHAR(20), FULLTEXT (x)
);
INSERT INTO t(x) VALUES ('abc'), ('def'), ('abc def'), ('abc abc');
ANALYZE TABLE t;

# ORDER_INDEX/NO_ORDER_INDEX are ignored for full text indexes.
--replace_regex $elide_costs
EXPLAIN SELECT /*+ NO_ORDER_INDEX(t x) */
 FTS_DOC_ID, MATCH (x) AGAINST ('abc') AS m FROM t
 WHERE MATCH (x) AGAINST ('abc')
 ORDER BY MATCH (x) AGAINST ('abc') DESC;

--replace_regex $elide_costs
EXPLAIN SELECT /*+ ORDER_INDEX(t x) */
 FTS_DOC_ID, MATCH (x) AGAINST ('abc') AS m FROM t
 WHERE MATCH (x) AGAINST ('abc')
 ORDER BY MATCH (x) AGAINST ('abc') DESC;

--replace_regex $elide_costs
EXPLAIN SELECT /*+ JOIN_INDEX(t x) */ * FROM t WHERE MATCH(x) AGAINST('abc');
--replace_regex $elide_costs
EXPLAIN SELECT /*+ NO_JOIN_INDEX(t x) */ * FROM t WHERE MATCH(x) AGAINST('abc');
DROP TABLE t;

# Check for NO_JOIN_INDEX
CREATE TABLE t1 (f1 INTEGER PRIMARY KEY);
EXPLAIN SELECT /*+ NO_JOIN_INDEX (t1) */ f1 FROM t1 WHERE f1 = 578;

DROP TABLE t1;
