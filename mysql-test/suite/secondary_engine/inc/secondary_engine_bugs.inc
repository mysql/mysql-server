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

--disable_query_log
UNINSTALL PLUGIN mock;
--enable_query_log
