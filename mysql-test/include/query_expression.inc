--echo #
--echo # Bug#33909696 WL#11350 Streaming doesn't happen, when UNION All used at top level
--echo #

--source include/elide_costs.inc

CREATE TABLE t(a INT);
CREATE TABLE t1(a INT);
CREATE TABLE r(a INT);

--replace_regex $elide_costs
EXPLAIN FORMAT = tree
   (SELECT * FROM t UNION SELECT * FROM t1)  UNION ALL (SELECT * FROM r);

--replace_regex $elide_costs
EXPLAIN FORMAT = tree
   (SELECT * FROM t UNION SELECT * FROM t1)  UNION ALL SELECT * FROM r
   UNION ALL SELECT * FROM t;

--replace_regex $elide_costs
EXPLAIN FORMAT = tree
   (SELECT * FROM t UNION SELECT * FROM t1)  UNION ALL SELECT * FROM r
   UNION DISTINCT SELECT * FROM t;

--replace_regex $elide_costs
EXPLAIN FORMAT = tree
   (SELECT * FROM t UNION SELECT * FROM t1)  UNION ALL SELECT * FROM r
   UNION ALL (SELECT * FROM t UNION DISTINCT SELECT * FROM r);

--replace_regex $elide_costs
EXPLAIN FORMAT = tree
   (SELECT * FROM t UNION SELECT * FROM t1)  UNION ALL SELECT * FROM r
   UNION ALL (SELECT * FROM t UNION ALL SELECT * FROM r);

DROP TABLE t, t1, r;
