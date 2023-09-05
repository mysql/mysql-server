--echo #
--echo # Bug#58463: Error Can't find record on SELECT with JOIN and ORDER BY
--echo #

# To produce the same query plan as in the bug report the first table
# must be stored in MyISAM.
CREATE TABLE t1 (
  pk INT NOT NULL,
  PRIMARY KEY (pk)
) ENGINE=MyISAM;

INSERT INTO t1 VALUES (2);

CREATE TABLE t2 (
  pk INT NOT NULL,
  i1 INT NOT NULL,
  i2 INT NOT NULL,
  c1 VARCHAR(1024) CHARACTER SET utf8mb3,
  PRIMARY KEY (pk),
  KEY k1 (i1)
);

INSERT INTO t2 VALUES (3, 9, 1, NULL);

-- disable_query_log
-- disable_result_log
ANALYZE TABLE t1;
ANALYZE TABLE t2;
-- enable_result_log
-- enable_query_log

let query=
SELECT i1
FROM t1 LEFT JOIN t2 ON t1.pk = t2.i2
WHERE t2.i1 > 5
AND t2.pk IS  NULL
ORDER BY i1;

eval EXPLAIN $query;
eval $query;

DROP TABLE t1, t2;


