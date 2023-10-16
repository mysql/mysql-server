--source include/elide_costs.inc

--disable_query_log
eval INSTALL PLUGIN mock SONAME '$MOCK_PLUGIN';
--enable_query_log

--echo #
--echo # Bug#33546083 - mysqld crash in Field::field_index
--echo #

CREATE TABLE t1 (
  pk int NOT NULL,
  col_varchar_255_key varchar(255),
  PRIMARY KEY (pk),
  KEY col_varchar_255_key (col_varchar_255_key)
) ENGINE=InnoDB SECONDARY_ENGINE=mock;

ALTER TABLE t1 SECONDARY_LOAD;
ANALYZE TABLE t1;

let $query=
SELECT (SELECT MAX(col_varchar_255_key ) FROM t1) AS field1
  FROM t1 AS alias1
  WHERE (alias1.col_varchar_255_key IN (SELECT 'f' UNION ALL SELECT 'r'))
  GROUP BY field1;

eval EXPLAIN $query;
eval $query;

DROP TABLE t1;

--echo #
--echo # Bug#34854369: Customer query hits assert(m_pq.is_valid()) failure
--echo #

CREATE TABLE t (x INT) SECONDARY_ENGINE MOCK;
INSERT INTO t VALUES (), (), (), ();
ALTER TABLE t SECONDARY_LOAD;
ANALYZE TABLE t;

# The Append path used to lack a row estimate.
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE SELECT * FROM t UNION ALL SELECT * FROM t;

# This query used to hit an assert failure during graph simplification
# when running with the hypergraph optimizer, caused by the missing
# row estimates.
SET optimizer_max_subgraph_pairs = 1;
WITH u AS (SELECT * FROM t UNION ALL SELECT * FROM t)
SELECT * FROM t AS t1 NATURAL JOIN t AS t2 NATURAL JOIN
              u AS u1 NATURAL JOIN u AS u2 NATURAL JOIN
              u AS u3 NATURAL JOIN u AS u4;
SET optimizer_max_subgraph_pairs = DEFAULT;

DROP TABLE t;

--echo #
--echo # Bug#35878975: mysqld crash - Assertion failure in OverflowBitset::Or
--echo #               at overflow_bitset.h
--echo #

CREATE TABLE t (x INT) SECONDARY_ENGINE MOCK;
ALTER TABLE t SECONDARY_LOAD;
SELECT 1 FROM t AS t1 LEFT OUTER JOIN t AS t2 LEFT JOIN t AS t3 ON TRUE
ON t3.x <=> 1 AND t2.x <> 1 AND t2.x <> 2 AND t2.x <> 3 AND t2.x <> 4 AND
t2.x <> 5 AND t2.x <> 6 AND t2.x <> 7 AND t2.x <> 8 AND t2.x <> 9 AND
t2.x <> 10 AND t2.x <> 11 AND t2.x <> 12 AND t2.x <> 13 AND t2.x <> 14 AND
t2.x <> 15 AND t2.x <> 16 AND t2.x <> 17 AND t2.x <> 18 AND t2.x <> 19 AND
t2.x <> 20 AND t2.x <> 21 AND t2.x <> 22 AND t2.x <> 23 AND t2.x <> 24 AND
t2.x <> 25 AND t2.x <> 26 AND t2.x <> 27 AND t2.x <> 28 AND t2.x <> 29 AND
t2.x <> 30 AND t2.x <> 31 AND t2.x <> 32 AND t2.x <> 33 AND t2.x <> 34 AND
t2.x <> 35 AND t2.x <> 36 AND t2.x <> 37 AND t2.x <> 38 AND t2.x <> 39 AND
t2.x <> 40 AND t2.x <> 41 AND t2.x <> 42 AND t2.x <> 43 AND t2.x <> 44 AND
t2.x <> 45 AND t2.x <> 46 AND t2.x <> 47 AND t2.x <> 48 AND t2.x <> 49 AND
t2.x <> 50 AND t2.x <> 51 AND t2.x <> 52 AND t2.x <> 53 AND t2.x <> 54 AND
t2.x <> 55 AND t2.x <> 56 AND t2.x <> 57 AND t2.x <> 58 AND t2.x <> 59 AND
t2.x <> 60 AND t2.x <> 61 AND t2.x <> 62 AND t2.x <> 63 AND t2.x <> 64;
DROP TABLE t;

--disable_query_log
UNINSTALL PLUGIN mock;
--enable_query_log
