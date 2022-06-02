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

--disable_query_log
UNINSTALL PLUGIN mock;
--enable_query_log
