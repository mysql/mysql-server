--source include/elide_costs.inc

--echo #
--echo # Bug#35686098 Assertion `n < size()' failed in Element_type& Mem_root_array_YY
--echo #

# At report time, the query used crash, later due to "commit 88d716a -
# Bug#35813111" it no longer failed, but fell back on tmp table
# de-duplication.  With this patch, it spills to disk - normal
# behavior since default buffer (set_operations_buffer_size) is too
# small for the data - but no longer falls back on tmp table
# de-duplication.

CREATE TABLE t1(
  c1 TEXT,
  c2 CHAR(255) DEFAULT NULL
);
LOAD DATA INFILE '../../std_data/t1_2cols.csv' INTO TABLE t1 FIELDS TERMINATED BY ',' LINES TERMINATED BY '\n';
ANALYZE TABLE t1;

SET optimizer_trace="enabled=on";

let $show_trace=
   SELECT JSON_PRETTY(JSON_EXTRACT(trace,"$.steps[*].join_execution"))
   FROM information_schema.optimizer_trace;

let $pattern=$elide_trace_costs_and_rows;
# elide some sorting statistics:
let $pattern=$pattern /num_initial_chunks_spilled_to_disk\": [0-9.]+/num_initial_chunks_spilled_to_disk\": "elided"/;
let $pattern=$pattern /peak_memory_used\": [0-9.]+/peak_memory_used\": "elided"/;
let $pattern=$pattern /num_rows_estimate\": [0-9.]+/num_rows_estimate\": "elided"/;
# Allow both 4 and 8 chunk files: the latter is seen with ASAN
let $pattern=$pattern /chunk files\": [48]/chunk files\": "elided"/;


SELECT MAX(c1) OVER (ORDER BY c2 ROWS CURRENT ROW) AS col FROM t1
INTERSECT DISTINCT
SELECT c2 = '' OR 447938560 FROM t1;

--replace_regex $pattern
--skip_if_hypergraph
eval $show_trace;

SET optimizer_trace=default;

DROP TABLE t1;

--echo #
--echo # Bug#35970620 hash_set_operations optimizer off assertion error
--echo #

# Prepare with hashing ON

PREPARE p0 FROM '(SELECT 3 AS three) EXCEPT (SELECT 1)';
SET SESSION optimizer_switch = 'hash_set_operations=off';
SET SESSION optimizer_trace = 'enabled=on';
EXECUTE p0;

SELECT JSON_PRETTY(JSON_EXTRACT(trace,
    '$.steps[*].join_execution.steps[*]."materialize for except"')) AS j
FROM information_schema.OPTIMIZER_TRACE;

SET SESSION optimizer_switch = 'hash_set_operations=on';
EXECUTE p0;

SELECT JSON_PRETTY(JSON_EXTRACT(trace,
    '$.steps[*].join_execution.steps[*]."materialize for except"')) AS j
FROM information_schema.OPTIMIZER_TRACE;

SET SESSION optimizer_switch = 'hash_set_operations=off';
EXECUTE p0;

SELECT JSON_PRETTY(JSON_EXTRACT(trace,
    '$.steps[*].join_execution.steps[*]."materialize for except"')) AS j
FROM information_schema.OPTIMIZER_TRACE;

SET SESSION optimizer_switch = 'hash_set_operations=on';
EXECUTE p0;

SELECT JSON_PRETTY(JSON_EXTRACT(trace,
    '$.steps[*].join_execution.steps[*]."materialize for except"')) AS j
FROM information_schema.OPTIMIZER_TRACE;

# Prepare with hashing OFF

SET SESSION optimizer_switch = 'hash_set_operations=off';
PREPARE p0 FROM '(SELECT 3 AS three) EXCEPT (SELECT 1)';
SET SESSION optimizer_switch = 'hash_set_operations=on';
EXECUTE p0;

SELECT JSON_PRETTY(JSON_EXTRACT(trace,
    '$.steps[*].join_execution.steps[*]."materialize for except"')) AS j
FROM information_schema.OPTIMIZER_TRACE;

SET SESSION optimizer_switch = 'hash_set_operations=off';

EXECUTE p0;
SELECT JSON_PRETTY(JSON_EXTRACT(trace,
    '$.steps[*].join_execution.steps[*]."materialize for except"')) AS j
FROM information_schema.OPTIMIZER_TRACE;

SET SESSION optimizer_switch = 'hash_set_operations=on';

EXECUTE p0;
SELECT JSON_PRETTY(JSON_EXTRACT(trace,
    '$.steps[*].join_execution.steps[*]."materialize for except"')) AS j
FROM information_schema.OPTIMIZER_TRACE;

SET SESSION optimizer_switch = 'hash_set_operations=off';

EXECUTE p0;
SELECT JSON_PRETTY(JSON_EXTRACT(trace,
    '$.steps[*].join_execution.steps[*]."materialize for except"')) AS j
FROM information_schema.OPTIMIZER_TRACE;

DROP PREPARE p0;

SET SESSION optimizer_trace = 'enabled=default';
SET SESSION optimizer_switch = 'hash_set_operations=default';

--echo #
--echo # Bug#36307622 Wrong result from query with WHERE integer IN (SELECT 2 EXCEPT SELECT 4)
--echo #
CREATE TABLE c (
  pk int NOT NULL AUTO_INCREMENT,
  col_datetime_key datetime DEFAULT NULL,
  col_varchar_key varchar(1) DEFAULT NULL,
  PRIMARY KEY (pk),
  KEY idx_c_col_datetime_key (col_datetime_key),
  KEY idx_c_col_varchar_key (col_varchar_key)
);

INSERT INTO c VALUES (1,'2022-10-30 08:18:58','o');
INSERT INTO c VALUES (2,'1998-01-19 17:27:57','䋠');
INSERT INTO c VALUES (3,'2015-09-01 16:34:18','X');
INSERT INTO c VALUES (4,'2020-08-29 15:09:33','m');
INSERT INTO c VALUES (5,'2018-07-01 22:36:45','d');
INSERT INTO c VALUES (6,'2028-02-07 02:02:10','q');
INSERT INTO c VALUES (7,'2016-02-04 17:29:46','8');
INSERT INTO c VALUES (8,'2037-07-02 21:02:05','0');
INSERT INTO c VALUES (9,'1970-08-03 04:25:41','旘');
INSERT INTO c VALUES (10,'1973-07-18 13:38:35','v');
INSERT INTO c VALUES (11,'1990-08-03 14:18:01','υ');
INSERT INTO c VALUES (12,'2036-07-05 20:41:55','ǆ');
INSERT INTO c VALUES (13,'2035-02-11 10:59:22','䩈');
INSERT INTO c VALUES (14,'1992-10-24 00:44:20','L');
INSERT INTO c VALUES (15,'1995-05-04 07:35:45','W');
INSERT INTO c VALUES (16,'2027-01-15 17:09:03','η');
INSERT INTO c VALUES (17,'1998-02-15 07:48:55','V');
INSERT INTO c VALUES (18,'1998-02-16 17:42:54','᥋');
INSERT INTO c VALUES (19,'1991-03-04 15:36:40','D');
INSERT INTO c VALUES (20,'1973-06-24 15:12:44','O');

SELECT pk FROM c WHERE pk IN (SELECT 2 EXCEPT SELECT 4);
SELECT pk FROM c WHERE pk IN (SELECT 2 EXCEPT SELECT 4) ORDER BY pk;

DROP TABLE c;

--echo #
--echo # Bug#36332697 IN predicate containing EXCEPT ALL set operation gives wrong result
--echo #
CREATE TABLE t(i INTEGER);
INSERT INTO t VALUES (1), (2), (3), (4), (5);
ANALYZE TABLE t;

--echo # This results in a single row of 2.
(SELECT 2 AS two UNION ALL SELECT 2)
EXCEPT ALL
SELECT 2;

--echo # Before the fix, this same set inside an IN predicate used to
--echo # return an empty result set even though one of the rows in t
--echo # has the value 2.
let $query =
SELECT i FROM t WHERE i IN ( (SELECT 2 UNION ALL SELECT 2)
                             EXCEPT ALL
                             SELECT 2 );
eval $query;
--replace_regex $elide_costs_and_time_and_row_estimate
--skip_if_hypergraph  # Different plan.
eval EXPLAIN ANALYZE $query;

--echo # Using tmp table index based de-duplication didn't help; same issue
SET optimizer_switch="hash_set_operations=off";
let $query =
SELECT i FROM t WHERE i IN ( (SELECT 2 UNION ALL SELECT 2)
                             EXCEPT ALL
                             SELECT 2 );
eval $query;
--replace_regex $elide_costs_and_time_and_row_estimate
--skip_if_hypergraph  # Different plan.
eval EXPLAIN ANALYZE $query;

SET optimizer_switch="hash_set_operations=default";

DROP TABLE t;

--echo #
--echo # Bug#36075756 ASAN crash on
--echo # MaterializeIterator<Profiler>::load_HF_row_into_hash_map()
--echo #
CREATE TABLE t1 (c1 INT, c2 TIME);

INSERT INTO t1 VALUES (4,'09:29:08'), (NULL,'19:11:10'), (2,'11:57:26'),
                      (6,'00:39:46'), (6,'03:28:15'),    (8,'06:44:18'),
                      (2,'14:36:39'), (6,'18:42:45'),    (8,'02:57:29'),
                      (3,'16:46:13');
CREATE VIEW view_t1 AS SELECT * FROM t1;

--echo # Used to assert with -DWITH_DEBUG=1 -DWITH_ASAN=1 (both required)
--echo # build on Oracle Linux Server 8
SELECT SUBSTRING(t.rep, 1, 4) AS sub, LENGTH(t.rep) AS len
FROM
( SELECT REPEAT(c1, c2) AS rep FROM view_t1
  EXCEPT DISTINCT
  SELECT DISTINCT RANK() OVER () FROM t1 ) AS t
ORDER BY len;

DROP VIEW view_t1;
DROP TABLE t1;

--echo #
--echo # Bug#36695371 wrong merge of derived table if unary nested
--echo #
CREATE TABLE t(pka INT NOT NULL AUTO_INCREMENT, a1 INT, a2 INT, PRIMARY KEY(pka));
INSERT INTO t(a1,a2) VALUES (1,3), (2,5), (3,7), (4,8), (5,9), (6,10), (7,11);
INSERT INTO t(a1,a2) SELECT a1,a2 FROM t;
ANALYZE TABLE t;
let $query =
SELECT * FROM ( (SELECT a1,a2 FROM t ORDER BY a1,a2 ASC)
                ORDER BY a2/a1 DESC LIMIT 7 ) AS derived;
eval $query;
--replace_regex $elide_costs
--skip_if_hypergraph  # Different plan.
eval EXPLAIN FORMAT=tree $query;
eval CREATE VIEW v AS $query;
SELECT * FROM v;
--replace_regex $elide_costs
--skip_if_hypergraph  # Different plan.
eval EXPLAIN FORMAT=tree SELECT * FROM v;
DROP VIEW v;
DROP TABLE t;

--echo #
--echo # Bug#36739383 Assertion `false' materialize_iterator::SpillState::
--echo #              read_next_row_secondary_over
--echo #
CREATE TABLE t(i int, b LONGTEXT);
INSERT INTO t VALUES (0, REPEAT('x', 120000)),
                     /* next row is too large on its own, drop spill
                        handling entirely revert to indexed tmp table
                        de-duplication */
                     (0, REPEAT('y', @@set_operations_buffer_size)),
                     (0, REPEAT('z', 12000));
ANALYZE TABLE t;
SET optimizer_trace='enabled=on';

let $query =
SELECT LENGTH(b) FROM (SELECT * FROM t INTERSECT SELECT * FROM t) derived;
--sorted_result
eval $query;

--skip_if_hypergraph # different trace
SELECT JSON_PRETTY(JSON_EXTRACT(
  trace,
  '$.steps[*].join_execution.steps[*]."materialize for intersect"')) AS info
FROM information_schema.OPTIMIZER_TRACE AS t;

TRUNCATE t;
INSERT INTO t VALUES (0, REPEAT('x', 12000)),
                     /* next row is too large because we can't fit two
                        such in hash map drop spill handling entirely
                        revert to indexed tmp table de-duplication */
                     (0, REPEAT('x', 140000)),
                     (0, REPEAT('z', 120000));
ANALYZE TABLE t;
--sorted_result
eval $query;

--skip_if_hypergraph # different trace
SELECT JSON_PRETTY(JSON_EXTRACT(
  trace,
  '$.steps[*].join_execution.steps[*]."materialize for intersect"')) AS info
FROM information_schema.OPTIMIZER_TRACE AS t;
SET optimizer_trace='enabled=default';
DROP TABLE t;

--echo #
--echo # Bug#37341055 MySQL version 9.1.0 crash with query thats works on version 8
--echo #
SELECT 'a' AS f1, 'b' AS f2, 'c' AS f3
UNION
SELECT 'a' AS f1 , NULL AS f2, 'c' AS f3
ORDER BY f2, f3;

--echo #
--echo # Bug#37742092 Limit number of spill sets in set operations spill-to-disk handling
--echo #

CREATE TABLE t1(
  c1 TEXT,
  c2 CHAR(255) DEFAULT NULL
);
LOAD DATA INFILE '../../std_data/t1_2cols.csv' INTO TABLE t1 FIELDS TERMINATED BY ',' LINES TERMINATED BY '\n';

INSERT INTO t1 SELECT CONCAT('1', c1), c2 FROM t1;
INSERT INTO t1 SELECT CONCAT('2', c1), c2 FROM t1;
INSERT INTO t1 SELECT CONCAT('3', c1), c2 FROM t1;
INSERT INTO t1 SELECT CONCAT('4', c1), c2 FROM t1;
ANALYZE TABLE t1;

SET set_operations_buffer_size = 16384;
SET optimizer_trace="enabled=on";

let $show_trace=
   SELECT JSON_PRETTY(JSON_EXTRACT(trace,"$.steps[*].join_execution"))
   FROM information_schema.optimizer_trace;

let $pattern=$elide_trace_costs_and_rows;
# elide some sorting statistics:
let $pattern=$pattern /num_initial_chunks_spilled_to_disk\": [0-9.]+/num_initial_chunks_spilled_to_disk\": "elided"/;
let $pattern=$pattern /peak_memory_used\": [0-9.]+/peak_memory_used\": "elided"/;
let $pattern=$pattern /num_rows_estimate\": [0-9.]+/num_rows_estimate\": "elided"/;
let $pattern=$pattern /to 65536/to xxxxx/;
let $pattern=$pattern /to 131072/to xxxxx/;
let $pattern=$pattern /\"chunk files\": [0-9]+/\"chunk files\": xxx/;
let $pattern=$pattern /\"chunk files\": [0-9]+/\"chunk files\": xxx/;

SELECT MAX(c1) OVER (ORDER BY c2 ROWS CURRENT ROW) AS col FROM t1
INTERSECT DISTINCT
SELECT c2 = '' OR 447938560 FROM t1;

--replace_regex $pattern
--skip_if_hypergraph
eval $show_trace;

SET set_operations_buffer_size= 131072; # trace recommended 65536, but
                                        # doesn't only works
                                        # sometimes, so double it

SELECT MAX(c1) OVER (ORDER BY c2 ROWS CURRENT ROW) AS col FROM t1
INTERSECT DISTINCT
SELECT c2 = '' OR 447938560 FROM t1;

--replace_regex $pattern
--skip_if_hypergraph
eval $show_trace;

# Trace also recommends increasing further to avoid spill totally,
# a factor of 64-128 (~ max # chunks)
SET set_operations_buffer_size= 131072*32;  # is too little, still spill

SELECT MAX(c1) OVER (ORDER BY c2 ROWS CURRENT ROW) AS col FROM t1
INTERSECT DISTINCT
SELECT c2 = '' OR 447938560 FROM t1;

--replace_regex $pattern
--skip_if_hypergraph
eval $show_trace;

SET set_operations_buffer_size= 131072*64;  # enough for no spill

SELECT MAX(c1) OVER (ORDER BY c2 ROWS CURRENT ROW) AS col FROM t1
INTERSECT DISTINCT
SELECT c2 = '' OR 447938560 FROM t1;

--replace_regex $pattern
--skip_if_hypergraph
eval $show_trace;

SET set_operations_buffer_size=default;   # spills

SELECT MAX(c1) OVER (ORDER BY c2 ROWS CURRENT ROW) AS col FROM t1
INTERSECT DISTINCT
SELECT c2 = '' OR 447938560 FROM t1;

--replace_regex $pattern
--skip_if_hypergraph
eval $show_trace;

SET optimizer_trace=default;

DROP TABLE t1;

--echo #
--echo # Bug#37804715 INTERSECT operation return wrong result
--echo #

--echo # original repro
CREATE TABLE t0(c0 INT) ;
CREATE TABLE t1(c1 INT PRIMARY KEY) ;
INSERT INTO t0(c0) VALUES(NULL);
INSERT INTO t1(c1) VALUES(1);
SELECT DISTINCT *
FROM t1 LEFT JOIN t0 ON t1.c1 = t0.c0
INTERSECT
SELECT DISTINCT *
FROM t1 RIGHT JOIN t0 ON t1.c1 = t0.c0;

--echo # Were also wrong: not nullable field in first set operand and
--echo # NULL in next set operand's field.
SELECT c1 FROM t1 INTERSECT SELECT c0 FROM t0;
SELECT c1 FROM t1 EXCEPT SELECT c0 FROM t0;

SELECT c1 FROM t1 INTERSECT ALL SELECT c0 FROM t0;
SELECT c1 FROM t1 EXCEPT ALL SELECT c0 FROM t0;

SELECT DISTINCT COUNT(*) AS c, SUM(t0.c0) AS s
FROM t1 LEFT JOIN t0 ON t1.c1 = t0.c0
INTERSECT
SELECT DISTINCT 1, t1.c1 FROM t1 LEFT JOIN t0 ON t1.c1 = t0.c0;

SELECT DISTINCT COUNT(*) AS c, SUM(t0.c0) AS s
FROM t1 LEFT JOIN t0 ON t1.c1 = t0.c0
GROUP BY t1.c1
INTERSECT
SELECT DISTINCT 1, t1.c1 FROM t1 LEFT JOIN t0 ON t1.c1 = t0.c0;

SELECT DISTINCT SUM(t0.c0) AS s
FROM t1 LEFT JOIN t0 ON t1.c1 = t0.c0
GROUP BY ROLLUP(t1.c1)
INTERSECT
SELECT DISTINCT t1.c1 FROM t1 LEFT JOIN t0 ON t1.c1 = t0.c0;

SELECT SUM(t0.c0) OVER () AS s
FROM t1 LEFT JOIN t0 ON t1.c1 = t0.c0
INTERSECT
SELECT DISTINCT t1.c1 FROM t1 LEFT JOIN t0 ON t1.c1 = t0.c0;

SET optimizer_switch='hash_set_operations=off';
SELECT c1 FROM t1 INTERSECT SELECT c0 FROM t0;
SELECT c1 FROM t1 EXCEPT SELECT c0 FROM t0;

SELECT c1 FROM t1 INTERSECT ALL SELECT c0 FROM t0;
SELECT c1 FROM t1 EXCEPT ALL SELECT c0 FROM t0;
SET optimizer_switch='hash_set_operations=default';

--echo # For intersect, the first operand's field(s) can also be
--echo # nullable even if result set's isn't:
SELECT null AS c1, null AS c2 INTERSECT SELECT 2,2;
SELECT c0 FROM t0 INTERSECT SELECT c1 FROM t1;

SET optimizer_switch='hash_set_operations=off';
SELECT null AS c1, null AS c2 INTERSECT SELECT 2,2;
SELECT c0 FROM t0 INTERSECT SELECT c1 FROM t1;

SET optimizer_switch='hash_set_operations=default';

--echo # Check nullable field in first or second operand with more
--echo # values.
TRUNCATE t0;
INSERT INTO t0 VALUES (NULL), (1), (1);
SELECT c0 FROM t0 INTERSECT SELECT c1 FROM t1;
SELECT c0 FROM t0 INTERSECT ALL SELECT c1 FROM t1;
SELECT c1 FROM t1 INTERSECT SELECT c0 FROM t0;
SELECT c1 FROM t1 INTERSECT ALL SELECT c0 FROM t0;
SELECT c0 FROM t0 EXCEPT SELECT c1 FROM t1;
SELECT c0 FROM t0 EXCEPT ALL SELECT c1 FROM t1;
SELECT c1 FROM t1 EXCEPT SELECT c0 FROM t0;
SELECT c1 FROM t1 EXCEPT ALL SELECT c0 FROM t0;

SET optimizer_switch='hash_set_operations=off';
SELECT c0 FROM t0 INTERSECT SELECT c1 FROM t1;
SELECT c0 FROM t0 INTERSECT ALL SELECT c1 FROM t1;
SELECT c1 FROM t1 INTERSECT SELECT c0 FROM t0;
SELECT c1 FROM t1 INTERSECT ALL SELECT c0 FROM t0;
SELECT c0 FROM t0 EXCEPT SELECT c1 FROM t1;
SELECT c0 FROM t0 EXCEPT ALL SELECT c1 FROM t1;
SELECT c1 FROM t1 EXCEPT SELECT c0 FROM t0;
SELECT c1 FROM t1 EXCEPT ALL SELECT c0 FROM t0;

SET optimizer_switch='hash_set_operations=default';

TRUNCATE t0;
TRUNCATE t1;
INSERT INTO t0(c0) VALUES(NULL), (NULL), (3), (5);
INSERT INTO t1(c1) VALUES(1), (3), (4);

SELECT AVG(c0), c0 FROM t0 GROUP BY c0
INTERSECT
SELECT c1, c1 FROM t1;

SELECT AVG(c0) OVER (PARTITION BY c0), c0 FROM t0 GROUP BY c0
INTERSECT
SELECT c1, c1 FROM t1;

DROP TABLE t0, t1;

--echo #
--echo # Bug#38799796: INTERSECT (SELECT NULL) incorrectly returns non-NULL rows
--echo #

SELECT 62 INTERSECT SELECT NULL;
SELECT 62 EXCEPT SELECT NULL;

--echo #
--echo # Bug#38735051: Intersection order
--echo #

SELECT NULL INTERSECT SELECT 1;
SELECT 1 INTERSECT SELECT NULL;

--echo #
--echo # Bug#38713332: INTERSECT bug: Null-handling inconsistency returns
--echo #               non-intersecting row from outer joins with NULL comp.
--echo #

CREATE TABLE t0(c0 FLOAT PRIMARY KEY, c1 FLOAT);
CREATE TABLE t1(c0 FLOAT, c1 FLOAT);
INSERT INTO t0(c0) VALUES(0);
INSERT INTO t1(c0) VALUES(NULL);

SELECT * FROM t0 LEFT JOIN t1 ON t0.c1 > t1.c1
INTERSECT
SELECT * FROM t0 RIGHT JOIN t1 ON t0.c1 > t1.c1;

SELECT * FROM t0 LEFT JOIN t1 ON t0.c1 > t1.c1;

SELECT * FROM t0 RIGHT JOIN t1 ON t0.c1 > t1.c1;

DROP TABLE t0, t1;
