# Skiping this test from Valgrind execution as per Bug-14627884
--source include/not_valgrind.inc
# Adding big test option for this test.
--source include/big_test.inc

--source include/elide_costs.inc

# t/index_merge_innodb.test
#
# Index merge tests
#
# Last update:
# 2006-08-07 ML test refactored (MySQL 5.1)
#               Main code of several index_merge tests
#                            -> include/index_merge*.inc
#               wrapper t/index_merge_innodb.test sources now several 
#               include/index_merge*.inc files
#

let $engine_type= InnoDB;
# According to Oracle: "InnoDB's estimate for the index cardinality
# depends on a pseudo random number generator (it picks up random
# pages to sample). After an optimization that was made in r2625 two
# EXPLAINs started returning a different number of rows (3 instead of
# 4)".
# Since hypergraph does not support traditional EXPLAIN format,
# we now use tree format for all the plans. Which ever plan has unstable
# rows and therefore costs, we do not display "rows/costs" as part of the
# plan.
# InnoDB does not support Merge tables (affects include/index_merge1.inc)
let $merge_table_support= 0;

--source include/index_merge1.inc
--source include/index_merge_ror.inc
--source include/index_merge2.inc

--source include/index_merge_2sweeps.inc
--source include/index_merge_ror_cpk.inc


--echo #
--echo # Bug#11747423 32254: INDEX MERGE USED UNNECESSARILY
--echo #
CREATE TABLE t1 (
  id INT NOT NULL PRIMARY KEY,
  id2 INT NOT NULL,
  id3 INT NOT NULL,
  KEY (id2),
  KEY (id3),
  KEY covering_index (id2,id3)
) ENGINE=InnoDB;

INSERT INTO t1 VALUES (0, 0, 0), (1, 1, 1), (2, 2, 2), (3, 3, 3), (4, 4, 4), (5, 5, 5), (6, 6, 6), (7, 7, 7);
INSERT INTO t1 SELECT id + 8, id2 + 8, id3 +8 FROM t1;
INSERT INTO t1 SELECT id + 16, 7, 0 FROM t1;

-- disable_query_log
-- disable_result_log
analyze table t1;
-- enable_result_log
-- enable_query_log

--replace_regex $elide_costs
EXPLAIN SELECT count(*) FROM t1 WHERE id2=7 AND id3=0;

DROP TABLE t1;

--echo #
--echo # Bug#21697002 ASSERTION `KEYS >= 0.0' FAILED
--echo #

CREATE TABLE t1 (
  c1 VARCHAR(2),
  d1 DATETIME NOT NULL
);

CREATE TABLE t2 (
  pk INTEGER NOT NULL,
  c1 VARCHAR(2),
  c2 VARCHAR(2),
  PRIMARY KEY (pk),
  KEY (c1),
  KEY (c2)
) PARTITION BY KEY(pk) PARTITIONS 2;

SELECT c1
FROM t1
WHERE c1 IN
(
  SELECT t2.c1
  FROM t2
  WHERE t2.c1 = 'f'
     OR t2.c2 < 'y'
)
AND t1.d1 IS NULL;

DROP TABLE t1, t2;

--echo #
--echo # Bug#36584358: Wrong result with index_merge_union=off
--echo #

CREATE TABLE t1 (
 f1 INTEGER,
 f2 INTEGER,
 KEY (f1 DESC,f2 DESC),
 KEY (f2 DESC,f1)
);
INSERT INTO t1 VALUES (1,NULL),(2,NULL),(3,NULL),
(NULL,NULL),(NULL,NULL),(NULL,NULL);
ANALYZE TABLE t1;

let query = SELECT f1 FROM t1 WHERE f1 OR f2;
eval EXPLAIN FORMAT=tree $query;
eval $query;

SET @optimizer_switch_saved = @@optimizer_switch;
SET optimizer_switch="index_merge_union=off";

# It would return wrong result earlier.
eval EXPLAIN FORMAT=tree $query;
eval $query;

SET optimizer_switch="index_merge_union=on";
SET @@optimizer_switch = @optimizer_switch_saved;


DROP TABLE t1;

--echo #
--echo # Bug#35206957 - EXPLAIN FORMAT=JSON
--echo #  "access_type" : "index_merge" missing field "used_key_parts"
--echo #

SET @@explain_json_format_version = 1;
SET @save_optimizer_switch = @@optimizer_switch;

CREATE TABLE t1 (
  pk INT NOT NULL AUTO_INCREMENT,
  a INT NOT NULL,
  b INT NOT NULL,
  c INT NOT NULL,
  d INT NOT NULL,
  PRIMARY KEY pk (pk),
  KEY idx_a (a),
  KEY idx_b (b),
  KEY idx_c (c),
  KEY idx_d (d)
);

INSERT INTO t1(a,b,c,d) VALUES
  (1,1,1,2),(2,1,2,3),(3,1,3,4),(4,1,4,5),(5,1,5,6),
  (6,2,1,2),(7,2,2,3),(8,2,3,4),(9,2,4,5),(10,2,5,6),
  (11,3,1,2),(12,3,2,3),(13,3,3,4),(14,3,4,5),(15,3,5,6),
  (16,4,1,2),(17,4,2,3),(18,4,3,4),(19,4,4,5),(20,4,5,6),
  (21,5,1,2),(22,5,2,3),(23,5,3,4),(24,5,4,5),(25,5,5,6),
  (25,6,6,6),(25,6,6,7);

ANALYZE TABLE t1;

--echo # intersect secondary indexes expose key parts.

EXPLAIN FORMAT=JSON INTO @plan UPDATE t1 SET a=2 WHERE b=6 AND c=6;

--skip_if_hypergraph  # Hypergraph does not support EXPLAIN JSONv1.
SELECT JSON_EXTRACT(@plan,
                    '$.query_block.table.key')
       AS key_name,
       JSON_EXTRACT(@plan,
                    '$.query_block.table.used_key_parts')
       AS used_key_parts,
       JSON_EXTRACT(@plan,
                    '$.query_block.table.key_length')
       AS key_length;

--echo # sort_union secondary indexes expose key parts.
SET optimizer_switch = 'index_merge_union=off';

EXPLAIN FORMAT=JSON INTO @plan SELECT * FROM t1 WHERE b = 6 OR c = 6;

--skip_if_hypergraph  # Hypergraph does not support EXPLAIN JSONv1.
SELECT JSON_EXTRACT(@plan,
                    '$.query_block.table.key')
       AS key_name,
       JSON_EXTRACT(@plan,
                    '$.query_block.table.used_key_parts')
       AS used_key_parts,
       JSON_EXTRACT(@plan,
                    '$.query_block.table.key_length')
       AS key_length;

SET optimizer_switch = @save_optimizer_switch;

--echo # union of intersect and range exposes key parts.

SET optimizer_switch = 'index_merge_intersection=on,index_merge_union=on';

EXPLAIN FORMAT=JSON INTO @plan SELECT * FROM t1 WHERE (b = 6 AND c = 6) OR d = 6;

--skip_if_hypergraph  # Hypergraph does not support EXPLAIN JSONv1.
SELECT JSON_EXTRACT(@plan,
                    '$.query_block.table.key')
       AS key_name,
       JSON_EXTRACT(@plan,
                    '$.query_block.table.used_key_parts')
       AS used_key_parts,
       JSON_EXTRACT(@plan,
                    '$.query_block.table.key_length')
       AS key_length;

SET optimizer_switch = @save_optimizer_switch;


--echo # composite index_merge exposes key parts.

ALTER TABLE t1
DROP INDEX idx_b,
DROP INDEX idx_c,
DROP INDEX idx_d,
ADD KEY idx_cd (c, d),
ADD KEY idx_bd (b, d);

EXPLAIN FORMAT=JSON INTO @plan
UPDATE /*+ INDEX_MERGE(t1 idx_cd, idx_bd) */ t1
SET a=2 WHERE b=6 AND c=6 AND d=6;

--skip_if_hypergraph  # Hypergraph does not support EXPLAIN JSONv1.
SELECT JSON_EXTRACT(@plan,
                    '$.query_block.table.key')
       AS key_name,
       JSON_EXTRACT(@plan,
                    '$.query_block.table.used_key_parts')
       AS used_key_parts,
       JSON_EXTRACT(@plan,
                    '$.query_block.table.key_length')
       AS key_length;

--echo # intersect with PRIMARY exposes key parts.

EXPLAIN FORMAT=JSON INTO @plan SELECT * FROM t1
WHERE a = 3 AND pk BETWEEN 10 AND 20;

--skip_if_hypergraph  # Hypergraph does not support EXPLAIN JSONv1.
SELECT JSON_EXTRACT(@plan,
                    '$.query_block.table.key')
       AS key_name,
       JSON_EXTRACT(@plan,
                    '$.query_block.table.used_key_parts')
       AS used_key_parts,
       JSON_EXTRACT(@plan,
                    '$.query_block.table.key_length')
       AS key_length;

--echo # sort_union orders PRIMARY key parts last.
SET optimizer_switch = 'index_merge_union=off';

EXPLAIN FORMAT=JSON INTO @plan SELECT * FROM t1
WHERE pk BETWEEN 10 AND 20 OR a = 3;

--skip_if_hypergraph  # Hypergraph does not support EXPLAIN JSONv1.
SELECT JSON_EXTRACT(@plan,
                    '$.query_block.table.key')
       AS key_name,
       JSON_EXTRACT(@plan,
                    '$.query_block.table.used_key_parts')
       AS used_key_parts,
       JSON_EXTRACT(@plan,
                    '$.query_block.table.key_length')
       AS key_length;

SET @@explain_json_format_version = DEFAULT;
SET optimizer_switch = @save_optimizer_switch;

DROP TABLE t1;
