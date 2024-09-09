--echo #
--echo # Start out by testing some simple in-memory inner hash joins.
--echo #

# Note: With the current cost model for the hypergraph optimizer
# we need at least ~6 rows in each table in order for the optimizer to select a hash join.
# Once the hypergraph optimizer supports the right type of hints, we should force it to
# use hash joins where possible rather than relying on the cost model.

# Test case is unstable in valgrind
--source include/not_valgrind.inc
--source include/elide_costs.inc

--echo # Join on two integer columns.
CREATE TABLE t1 (col1 INTEGER);
CREATE TABLE t2 (col1 INTEGER);
INSERT INTO t1 VALUES (1), (3), (5), (7), (7), (7);
INSERT INTO t2 VALUES (1), (2), (5), (6), (6), (6);
ANALYZE TABLE t1, t2;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree
  SELECT t1.col1, t2.col1 FROM t1 JOIN t2 ON t1.col1 = t2.col1 ORDER BY t1.col1;
SELECT t1.col1, t2.col1 FROM t1 JOIN t2 ON t1.col1 = t2.col1 ORDER BY t1.col1;
DROP TABLE t1, t2;

--echo # Join on a integer column and a string column.
CREATE TABLE t1 (col1 INTEGER);
CREATE TABLE t2 (col1 VARCHAR(255));
INSERT INTO t1 VALUES (1), (3), (5), (7), (7), (7);
INSERT INTO t2 VALUES (1), (2), (5), (6), (6), (6);
ANALYZE TABLE t1, t2;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree
  SELECT t1.col1, t2.col1 FROM t1 JOIN t2 ON t1.col1 = t2.col1 ORDER BY t1.col1;
SELECT t1.col1, t2.col1 FROM t1 JOIN t2 ON t1.col1 = t2.col1 ORDER BY t1.col1;
DROP TABLE t1, t2;

--echo # Join on two datetime columns.
CREATE TABLE t1 (col1 DATETIME(6));
CREATE TABLE t2 (col1 DATETIME(6));
INSERT INTO t1 VALUES ('2018-01-01 00:00:00.000000'),
                      ('2018-01-01 00:00:00.000001'),
                      ('2018-01-02 00:00:00.000000'),
                      ('2018-01-02 00:00:00.000001'),
                      (NULL), (NULL);
INSERT INTO t2 VALUES ('2018-01-01 00:00:00.000000'),
                      ('2018-01-01 00:00:00.000002'),
                      ('2018-01-02 00:00:00.000001'),
                      ('2019-01-02 00:00:00.000001'),
                      (NULL), (NULL), (NULL);
ANALYZE TABLE t1, t2;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree
  SELECT t1.col1, t2.col1 FROM t1 JOIN t2 ON t1.col1 = t2.col1;
SELECT t1.col1, t2.col1 FROM t1 JOIN t2 ON t1.col1 = t2.col1 ORDER BY t1.col1;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree
  SELECT t1.col1, t2.col1 FROM t1 JOIN t2 ON t1.col1 <=> t2.col1;
--sorted_result
SELECT t1.col1, t2.col1 FROM t1 JOIN t2 ON t1.col1 <=> t2.col1 ORDER BY t1.col1;
DROP TABLE t1, t2;

--echo # Join on a string and datetime column, where datetime comparison is
--echo # picked.
CREATE TABLE t1 (a DATETIME);
INSERT INTO t1 VALUES ('2001-01-01 00:00:00'), (NULL), ('2002-01-01 00:00:00'),
('2002-01-01 00:00:00'), ('2002-01-01 00:00:00'), ('2002-01-01 00:00:00');

CREATE TABLE t2 (b VARCHAR(64));
INSERT INTO t2 VALUES ('2001#01#01'), (NULL), (NULL), ('1999-01-01'), ('1999-01-01'), ('1999-01-01');
ANALYZE TABLE t1, t2;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t1, t2 WHERE a=b;
SELECT * FROM t1, t2 WHERE a=b;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t1, t2 WHERE a<=>b;
--sorted_result
SELECT * FROM t1, t2 WHERE a<=>b;
DROP TABLE t1, t2;

--echo # Join on two double columns.
CREATE TABLE t1 (col1 DOUBLE);
CREATE TABLE t2 (col1 DOUBLE);
INSERT INTO t1 VALUES (0), (1.1), (3.3), (5.5), (7.7), (NULL);
INSERT INTO t2 VALUES (0), (1.1), (1.11), (5.5), (6.6), (NULL);
ANALYZE TABLE t1, t2;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree
  SELECT t1.col1, t2.col1 FROM t1 JOIN t2 ON t1.col1 = t2.col1;
SELECT t1.col1, t2.col1 FROM t1 JOIN t2 ON t1.col1 = t2.col1 ORDER BY t1.col1;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree
  SELECT t1.col1, t2.col1 FROM t1 JOIN t2 ON t1.col1 <=> t2.col1;
--sorted_result
SELECT t1.col1, t2.col1 FROM t1 JOIN t2 ON t1.col1 <=> t2.col1;
DROP TABLE t1, t2;

--echo # Join on two decimal columns.
CREATE TABLE t1 (col1 DECIMAL(6, 2));
CREATE TABLE t2 (col1 DECIMAL(6, 2));
INSERT INTO t1 VALUES (0), (1.1), (3.3), (5.5), (7.7), (NULL);
INSERT INTO t2 VALUES (0), (1.1), (1.10), (5.5), (6.6), (NULL);
ANALYZE TABLE t1, t2;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree
  SELECT t1.col1, t2.col1 FROM t1 JOIN t2 ON t1.col1 = t2.col1;
SELECT t1.col1, t2.col1 FROM t1 JOIN t2 ON t1.col1 = t2.col1 ORDER BY t1.col1;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree
  SELECT t1.col1, t2.col1 FROM t1 JOIN t2 ON t1.col1 <=> t2.col1;
--sorted_result
SELECT t1.col1, t2.col1 FROM t1 JOIN t2 ON t1.col1 <=> t2.col1;
DROP TABLE t1, t2;

--echo # See that comparison between decimal and bigint works well. The main
--echo # challenge is that decimals with different amount of leading/trailing
--echo # zeroes should compare equally.
CREATE TABLE t1 (col1 BIGINT);
CREATE TABLE t2 (col1 DECIMAL(64,30));
INSERT INTO t1 VALUES (0), (1), (5), (10), (10), (NULL);
INSERT INTO t2 VALUES (0), (0.1), (0.000000000000000000000000000001),
                      (5.000000000000000000000000000000), (6), (NULL);
ANALYZE TABLE t1, t2;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t1,t2 WHERE t1.col1 = t2.col1;
SELECT * FROM t1,t2 WHERE t1.col1 = t2.col1;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t1,t2 WHERE t1.col1 <=> t2.col1;
--sorted_result
SELECT * FROM t1,t2 WHERE t1.col1 <=> t2.col1;
DROP TABLE t1, t2;

CREATE TABLE t1 (col1 DECIMAL(5));
CREATE TABLE t2 (col1 BIGINT);
INSERT INTO t1 VALUES (1), (2), (2), (2), (2), (NULL);
INSERT INTO t2 VALUES (1), (0), (3), (3), (3), (NULL);
ANALYZE TABLE t1, t2;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t1,t2 where t1.col1=t2.col1;
SELECT * FROM t1,t2 where t1.col1=t2.col1;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t1,t2 where t1.col1<=>t2.col1;
--sorted_result
SELECT * FROM t1,t2 where t1.col1<=>t2.col1;
DROP TABLE t1, t2;


--echo # Bit fields, which is a bit different depending on the storage engine.
create table t1 (id1 int, b1 bit(1)) engine = myisam;
create table t2 (id2 int, b2 bit(1)) engine = myisam;
insert into t1 values (2, 0), (3, 1), (4, 0), (4, 0), (4, 0), (null, null);
insert into t2 values (2, 1), (3, 0), (5, 0), (5, 0), (5, 0), (null, null);
ANALYZE TABLE t1, t2;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t1, t2 WHERE id1 = id2;
SELECT id1, HEX(b1), id2, HEX(b2) FROM t1, t2 WHERE id1 = id2;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t1, t2 WHERE id1 <=> id2;
--sorted_result
SELECT id1, HEX(b1), id2, HEX(b2) FROM t1, t2 WHERE id1 <=> id2;
DROP TABLE t1, t2;

create table t1 (id1 int, b1 bit(64)) engine = innodb;
create table t2 (id2 int, b2 bit(64)) engine = innodb;
insert into t1 values (2, 0), (3, 2), (4, 4), (4, 4), (4, 4), (null, null);
insert into t2 values (2, 2), (3, 0), (5, 4), (5, 4), (5, 4), (null, null);
ANALYZE TABLE t1, t2;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t1, t2 WHERE id1 = id2;
SELECT id1, HEX(b1), id2, HEX(b2) FROM t1, t2 WHERE id1 = id2;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t1, t2 WHERE id1 <=> id2;
--sorted_result
SELECT id1, HEX(b1), id2, HEX(b2) FROM t1, t2 WHERE id1 <=> id2;
DROP TABLE t1, t2;

--echo # See that we handle NULL values properly.
CREATE TABLE t1 (col1 VARCHAR(255));
CREATE TABLE t2 (col1 VARCHAR(255));
INSERT INTO t1 VALUES (NULL), ('a'), ('a'), ('a'), ('a'), ('a');
INSERT INTO t2 VALUES (''), (NULL), ('x'), ('y'), ('y'), ('y');
ANALYZE TABLE t1, t2;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t1, t2 WHERE t1.col1 = t2.col1;
SELECT * FROM t1, t2 WHERE t1.col1 = t2.col1;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t1, t2 WHERE t1.col1 <=> t2.col1;
SELECT * FROM t1, t2 WHERE t1.col1 <=> t2.col1;
DROP TABLE t1,t2;

--echo #
--echo # Now, do some queries where we end up with a GRACE hash join. That is,
--echo # the right table of the join is bigger than the join_buffer_size.
--echo #
CREATE TABLE t1 (col1 BIGINT);
CREATE TABLE t2 (col1 BIGINT);

INSERT INTO t1 SELECT 1;
INSERT INTO t1 SELECT col1 + 1 FROM t1;
INSERT INTO t1 SELECT col1 + 2 FROM t1;
INSERT INTO t1 SELECT col1 + 4 FROM t1;
INSERT INTO t1 SELECT col1 + 8 FROM t1;
INSERT INTO t1 SELECT col1 + 16 FROM t1;
INSERT INTO t1 SELECT col1 + 32 FROM t1;
INSERT INTO t1 SELECT col1 + 64 FROM t1;
INSERT INTO t1 SELECT col1 + 128 FROM t1;
INSERT INTO t1 SELECT col1 + 256 FROM t1;
INSERT INTO t1 SELECT col1 + 512 FROM t1;
INSERT INTO t2 SELECT col1 FROM t1;

ANALYZE TABLE t1, t2;

SET join_buffer_size = 2048;  # 2 kB.

let $hash_join_file_operations =
  SELECT COUNT_STAR > 0 FROM performance_schema.file_summary_by_event_name
    WHERE event_name LIKE '%hash_join%';

# Mask out the cost estimates, since they will differ with innodb_page_size.
--replace_regex $elide_costs
EXPLAIN FORMAT=tree
  SELECT SUM(t1.col1), SUM(t2.col1) FROM t1, t2 WHERE t1.col1 = t2.col1;

TRUNCATE performance_schema.file_summary_by_event_name;

eval $hash_join_file_operations;
SELECT SUM(t1.col1), SUM(t2.col1) FROM t1, t2 WHERE t1.col1 = t2.col1;
--replace_regex $elide_costs
eval $hash_join_file_operations;

SET join_buffer_size = DEFAULT;
DROP TABLE t1,t2;

--echo # See that spill to disk (GRACE hash join) works with all kind of
--echo # data types.
CREATE TABLE t1 (
  str_col VARCHAR(255),
  blob_col LONGBLOB,
  text_col LONGTEXT,
  bit_col BIT(64),
  tinyint_col TINYINT,
  smallint_col SMALLINT,
  mediumint_col MEDIUMINT,
  int_col INTEGER,
  bigint_col BIGINT,
  float_col FLOAT,
  double_col DOUBLE,
  decimal_col DECIMAL(65, 30),
  year_col YEAR,
  date_col DATE,
  time_col TIME(6),
  datetime_col DATETIME(6),
  timestamp_col TIMESTAMP(6),
  json_col JSON,
  geometry_col GEOMETRY
);

# Set time_zone since limits for TIMESTAMP is in UTC.
SET time_zone = '+00:00';

INSERT INTO t1 VALUES (
  '',
  '',
  '',
  b'0000000000000000000000000000000000000000000000000000000000000000',
  -128,
  -32768,
  -8388608,
  -2147483648,
  -9223372036854775808,
  -3.402823466E+38,
  -1.7976931348623157E+308,
  '-99999999999999999999999999999999999.999999999999999999999999999999',
  1901,
  '1000-01-01',
  '-838:59:59.000000',
  '1000-01-01 00:00:00.000000',
  '1970-01-01 00:00:01.000000',
  '{}',
  ST_GeomFromText('GEOMETRYCOLLECTION()')
);

INSERT INTO t1 VALUES (
  'a very long and interesting string',
  'a very long and interesting blob',
  'a very long and interesting text',
  b'1111111111111111111111111111111111111111111111111111111111111111',
  127,
  32767,
  8388607,
  2147483647,
  9223372036854775807,
  3.402823466E+38,
  1.7976931348623157E+308,
  '99999999999999999999999999999999999.999999999999999999999999999999',
  2155,
  '9999-12-31',
  '838:59:59.000000',
  '9999-12-31 23:59:59.999999',
  '2038-01-19 03:14:07.999999',
  '{"key": [1, 2, 3]}',
  ST_GeomFromText('GEOMETRYCOLLECTION(POINT(1 2), POINT(3 4))')
);

INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 VALUES (); # One row with only NULL values for completeness.

SET join_buffer_size = 99968;

--echo # Just do a few aggregations for sanity checking. We don't want to
--echo # pollute the result log with thousands of lines with binary data.
let $query1 = SELECT
  COUNT(*),
  SUM(LENGTH(t1.text_col)),
  SUM(t2.bigint_col)
FROM
  t1,
  t1 AS t2
WHERE
  t1.int_col = t2.int_col
ORDER BY
  t1.int_col;

# Same query with <=>.
let $query2 = SELECT
  COUNT(*),
  SUM(LENGTH(t1.text_col)),
  SUM(t2.bigint_col)
FROM
  t1,
  t1 AS t2
WHERE
  t1.int_col <=> t2.int_col
ORDER BY
  t1.int_col;

ANALYZE TABLE t1;

# Mask out the cost estimates, since they will differ with innodb_page_size.
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query1;
eval $query1;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query2;
eval $query2;

DROP TABLE t1;
SET join_buffer_size = DEFAULT;
SET time_zone = DEFAULT;

--echo #
--echo # A query where we end up with a weedout + hash join. This forces hash
--echo # join to keep the row ID for each row, so that the duplicate removal
--echo # works.
--echo #

# It seems like the intention of this test case is to test that the following type of plan works,
#
# -> Remove duplicate t2 rows using temporary table (weedout)  (rows=2)
#     -> Inner hash join (t1.i = t2.i)  (rows=2)
#         -> Table scan on t1  (rows=3)
#         -> Hash
#             -> Table scan on t2  (rows=2)
#
# However, the hypergraph optimizer seems to prefer either using a hash semijoin
# or a NLJ with sorting for duplicate removal.

SET optimizer_switch="materialization=off,firstmatch=off";
CREATE TABLE t1 (i BIGINT);
CREATE TABLE t2 (i BIGINT);
INSERT INTO t1 VALUES (1), (2), (3);
INSERT INTO t2 VALUES (2), (3), (4);
ANALYZE TABLE t1, t2;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t2 WHERE (t2.i) IN (SELECT t1.i FROM t1);

SELECT * FROM t2 WHERE (t2.i) IN (SELECT t1.i FROM t1);

--echo # Increase the data volume, and reduce the join_buffer_size, in order to
--echo # test that we can keep the row ID in case of GRACE hash join as well.
let $count=9;
while ($count)
{
  eval INSERT INTO t1 SELECT * FROM t1;
  eval INSERT INTO t2 SELECT * FROM t2;
  dec $count;
}
ANALYZE TABLE t1, t2;

SET join_buffer_size = 2048;  # 2 kB.

let $hash_join_file_operations =
  SELECT COUNT_STAR > 0 FROM performance_schema.file_summary_by_event_name
    WHERE event_name LIKE '%hash_join%';

# Mask out the cost estimates, since they will differ with innodb_page_size.
--replace_regex $elide_costs
EXPLAIN FORMAT=tree
  SELECT COUNT(*) FROM t2 WHERE (t2.i) IN (SELECT t1.i FROM t1);
TRUNCATE performance_schema.file_summary_by_event_name;

eval $hash_join_file_operations;
SELECT COUNT(*) FROM t2 WHERE (t2.i) IN (SELECT t1.i FROM t1);
# Hypergraph does not use weedout, so does not need row ID, and thus
# does not spill to disk.
eval $hash_join_file_operations;

DROP TABLE t1, t2;
SET join_buffer_size = DEFAULT;
SET optimizer_switch = DEFAULT;

--echo # Test a query with multiple predicates; some of which are
--echo # NULL-safe equalities and others are regular equalities.
CREATE TABLE t (a INT, b INT, c INT, d INT);
INSERT INTO t VALUES
(1, 2, 3, 4), (NULL, 2, 3, 4), (1, NULL, 3, 4), (1, 2, NULL, 4),
(1, 2, 3, NULL), (NULL, NULL, NULL, NULL), (1, NULL, 3, NULL),
(0, 0, 0, 0), (NULL, 0, NULL, 0), (0, NULL, 0, NULL);
ANALYZE TABLE t;
--replace_regex $elide_costs
EXPLAIN FORMAT=TREE
SELECT * FROM t t1, t t2
WHERE t1.a = t2.a AND t1.b <=> t2.b AND t1.c = t2.c AND t1.d <=> t2.d;
--sorted_result
SELECT * FROM t t1, t t2
WHERE t1.a = t2.a AND t1.b <=> t2.b AND t1.c = t2.c AND t1.d <=> t2.d;
DROP TABLE t;

--echo # Test a case where the RAND() function is pushed as late as possible in
--echo # the join. The optimizer ends up rewriting t1.col1 = FLOOR(...) to
--echo # t2.col1 = FLOOR(...), so this test case ensures that the executor is
--echo # able to put the condition after the join. FLOOR and division/addition
--echo # make this query deterministic.
CREATE TABLE t1 (col1 INTEGER);
CREATE TABLE t2 (col1 INTEGER);
INSERT INTO t1 VALUES (1), (2), (3), (3), (3), (3);
INSERT INTO t2 VALUES (1), (2), (4), (4), (4), (4);
ANALYZE TABLE t1, t2;

let $query = SELECT
  t1.col1,
  t2.col1
FROM
  t1,
  t2
WHERE
  t1.col1 = t2.col1
  AND t1.col1 = FLOOR(RAND() / 2 + 2);

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;
DROP TABLE t1, t2;

--echo # Ensure that the hash join picks the correct fields and tables when both
--echo # sides of the join condition are from the same source table.
CREATE TABLE c (
  col1 varchar(1)
) ENGINE = myisam;

INSERT INTO c VALUES ('a'), ('b'), ('c'), ('d'), ('e'), ('f');
ANALYZE TABLE c;

let $query = SELECT *
FROM
  (SELECT * FROM c) AS table1
  JOIN (SELECT * FROM c) AS table2
  ON table2.col1 = table1.col1;

--replace_regex $elide_costs
eval EXPLAIN format=tree $query;
eval $query;
DROP TABLE c;

--echo # This query ends up with a BNL between t3 and t2. Ensure that we don't
--echo # end up with a hash join like:
--echo #
--echo #  -> Constant row from <subquery2>
--echo #    -> Materialize with deduplication
--echo #        -> HashJoin inner join (t3.i = '2')
--echo #            -> Table scan on t2
--echo #            -> Table scan on t3
--echo #
--echo # We don't want a join condition on a constant, so it should be pushed as
--echo # a filter.
SET optimizer_switch='firstmatch=off';

CREATE TABLE t1 (i INTEGER) ENGINE = MyISAM;
CREATE TABLE t2 (i INTEGER) ENGINE = MyISAM;
CREATE TABLE t3 (i INTEGER) ENGINE = MyISAM;

INSERT INTO t1 VALUES (2), (3), (3), (3), (3), (3);
INSERT INTO t2 VALUES (2), (4), (4), (4), (4), (4);
ANALYZE TABLE t1, t2;

--echo # Note: plan change with new hypergraph cost model.
--echo # Seems to avoid the undesirable plan.
--replace_regex $elide_costs
EXPLAIN FORMAT=tree
  SELECT * FROM t1 WHERE (t1.i) IN  (SELECT t3.i FROM t2 STRAIGHT_JOIN t3);
SELECT * FROM t1 WHERE (t1.i) IN  (SELECT t3.i FROM t2 STRAIGHT_JOIN t3);

DROP TABLE t1,t2,t3;
SET optimizer_switch=DEFAULT;

--echo # A bit more complicated join condition where we have multiple join
--echo # conditions, and one of them is an expression.
CREATE TABLE t1 (a INTEGER, b INTEGER);
INSERT INTO t1 (a) VALUES (1),(2);
CREATE TABLE t3 (a INTEGER, b INTEGER);
INSERT INTO t3 VALUES (1, 10), (1, 11), (2, 10), (2, 11);
ANALYZE TABLE t1, t3;

let $query = SELECT * FROM t1, t3 WHERE t3.b = t1.a + 9 AND t3.a = t1.a;

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

DROP TABLE t1,t3;


--echo # Ensure that outer joins doesn't degrade into a nested loop,
--echo # but still uses join buffering.
CREATE TABLE t1 (col1 INTEGER);
CREATE TABLE t2 (col1 INTEGER);
INSERT INTO t1 VALUES (1), (2), (3), (3), (3), (3), (3), (3), (3), (3);
INSERT INTO t2 VALUES (2), (4), (4), (4), (4), (4), (4), (4), (4), (4);
ANALYZE TABLE t1, t2;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t1 LEFT JOIN t2 ON t1.col1 = t2.col1;
DROP TABLE t1, t2;

--echo # See that we can replace a BNL with hash join, even if we have extra
--echo # join conditions that are not equi-join conditions. The result should be
--echo # that the non-equi-join conditions should be attached as a filter after
--echo # the join.
CREATE TABLE t1 (col1 INTEGER, col2 INTEGER);
CREATE TABLE t2 (col1 INTEGER, col2 INTEGER);
INSERT INTO t1 VALUES (1, 1), (2, 2), (3, 3), (4, 3), (4, 3), (4, 3);
INSERT INTO t2 VALUES (1, 1), (2, 4), (3, 6), (5, 3), (5, 3), (5, 3);
ANALYZE TABLE t1, t2;

--replace_regex $elide_costs
EXPLAIN FORMAT=tree
SELECT * FROM t1 JOIN t2 ON t1.col1 = t2.col1 AND t1.col2 < t2.col2;

SELECT * FROM t1 JOIN t2 ON t1.col1 = t2.col1 AND t1.col2 < t2.col2;

DROP TABLE t1, t2;


CREATE TABLE t1 (col1 BIGINT);
INSERT INTO t1 VALUES (1), (1), (1), (1), (1), (1), (1), (1), (1), (1);
ANALYZE TABLE t1;
--echo # Should use hash joins only, since there are no indexes that could
--echo # help speed up nested loop joins.
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT SUM(t1.col1)
  FROM t1, t1 t2, t1 t3, t1 t4, t1 t5, t1 t6;
SELECT SUM(t1.col1) FROM t1, t1 t2, t1 t3, t1 t4, t1 t5, t1 t6;
DROP TABLE t1;

--echo # Test that comparison between FLOAT and DOUBLE works as expected if
--echo # given an explicit number of decimals.
CREATE TABLE t1 (col1 FLOAT(5,2), col2 DOUBLE(15,2));
INSERT INTO t1 VALUES (1.01, 1.01);
SELECT * FROM t1 a, t1 b WHERE a.col1 = b.col2;
DROP TABLE t1;

--echo # The point of the following test is to see that if the innermost hash
--echo # join returns zero rows, the outermost hash join should not scan the
--echo # probe table.
CREATE TABLE t1 (col1 INT);
CREATE TABLE t2 (col1 INT);
CREATE TABLE t3 (col1 INT);

INSERT INTO t1 VALUES (1), (2), (3), (4), (5), (6), (7), (8), (9);
INSERT INTO t2 VALUES (1), (2), (3), (4), (5), (6), (7), (8), (9);
INSERT INTO t3 VALUES (1), (2), (3), (4), (5), (6), (7), (8), (9);
ANALYZE TABLE t1, t2, t3;

let $query = SELECT STRAIGHT_JOIN * FROM t1
  JOIN t2 ON t1.col1 + 999 = t2.col1
  JOIN t3 ON t2.col1 = t3.col1;

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;

--disable_query_log
--disable_result_log
# The first time we query session_status, the server will query additional
# dictionary tables to get the necessary metadata. This extra query will be
# counted in the performance schema, so we run a query against session_status
# once here so we are sure that the necessary metadata is fetched.
SELECT SUM(variable_value) AS Total_handler_reads
   FROM performance_schema.session_status
   WHERE variable_name LIKE 'Handler_read%';
FLUSH STATUS;
eval $query;
--enable_result_log
--enable_query_log

--echo # The test doesn't specify what we expect here, rendering it meaningless and unmaintainable.
SELECT SUM(variable_value) AS Total_handler_reads
   FROM performance_schema.session_status
   WHERE variable_name LIKE 'Handler_read%';

DROP TABLE t1, t2, t3;

--echo #
--echo # Bug#29898802	WL#2241: SIG6 IN HASH_JOIN_BUFFER::LOADINTOTABLEBUFFERS()
--echo #              AT HASH_JOIN_BUFFER.CC
--echo #
CREATE TABLE t1 (
  pk int NOT NULL AUTO_INCREMENT,
  col_varchar varchar(1),
  col_varchar_key varchar(1),
  PRIMARY KEY (pk),
  KEY idx_CC_col_varchar_key (col_varchar_key)
);

INSERT INTO t1 VALUES (1,'n','X'), (2,'Y','8'), (3,'R','l'), (4,'a','b'), (5,'c','d'), (6,'e','f');

let $query = SELECT
  t1.col_varchar_key AS field1
FROM
  (t1, t1 as alias1)
WHERE
  NOT
    EXISTS(
      SELECT
        alias2.col_varchar_key
      FROM
        t1 AS alias2
      WHERE
        alias2.col_varchar_key >= t1.col_varchar
    )
GROUP BY
  field1;

ANALYZE TABLE t1;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

DROP TABLE t1;

--echo # See that typed arrays are handled as blobs. That is, we do not try to
--echo # allocate 4GB of memory during the hash join.
CREATE TABLE t1 (
  col_int_key INTEGER,
  col_json JSON,
  KEY mv_idx ((CAST(col_json->'$[*]' AS CHAR(40) ARRAY)))
);

INSERT INTO t1 VALUES (NULL, '[1]'), (4, '[1]'), (1, '[2]');

CREATE TABLE t2(col_int INTEGER);

INSERT INTO t2 VALUES (1), (2), (3), (11), (12);

let $query = SELECT t1.col_int_key AS field1, t2.col_int AS field2 FROM t2
  JOIN t1 ON 1 WHERE (CAST("1" AS JSON) MEMBER OF( t1.col_json->'$[*]'));

ANALYZE TABLE t1, t2;

# Mask out the cost estimates, since they will differ with innodb_page_size.
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;

# We do not care about the result. The only thing we want to see is that we do
# not error out with "out of memory".
--disable_result_log
eval $query;
--enable_result_log
DROP TABLE t1,t2;

--echo #
--echo # Bug#29906372 WL#2241: SIG6 IN HASH_JOIN_BUFFER::STOREFROMTABLEBUFFERS
--echo #              AT HASH_JOIN_BUFFER.CC
--echo #
CREATE TABLE a (
  pk INTEGER NOT NULL AUTO_INCREMENT,
  col_varchar VARCHAR(1),
  col_varchar_key VARCHAR(1),
  PRIMARY KEY (pk),
  KEY varchar_key (col_varchar_key)
);

CREATE TABLE b (
  pk INTEGER NOT NULL AUTO_INCREMENT,
  col_varchar VARCHAR(1),
  col_varchar_key VARCHAR(1),
  PRIMARY KEY (pk),
  KEY varchar_key (col_varchar_key)
);

INSERT INTO a VALUES (1, 'N', '0');
INSERT INTO b VALUES (1, '8', 'r'), (2, 'v', 'C'), (3, 'b', 'p'), (4, '7', 'W');

let $query = SELECT 1 FROM (b AS table1
  INNER JOIN a AS table2 ON table2.pk = table1.pk OR table1.col_varchar < 'D')
  WHERE (NOT EXISTS
    (SELECT 1 FROM (b AS alias3 STRAIGHT_JOIN a AS alias4
      ON alias4.col_varchar = alias3.col_varchar_key)
      WHERE alias3.pk >= table1.pk));

ANALYZE TABLE a, b;
--echo # It will be difficult to trigger the same plan using the hypergraph optimizer
--echo # and to keep it stable under cost model changes. We avoid changing the original
--echo # test so that it at least tests for the bug with the original optimizer.
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
# The result is not important; we only want to see that we do not assert.
--disable_result_log
eval $query;
--enable_result_log
DROP TABLE a, b;

--echo #
--echo # Bug#29947439 WL#2241: FLOATING POINT EXCEPTION: INITIALIZECHUNKFILES AT
--echo #              HASH_JOIN_ITERATOR.CC
--echo #
# We want to get the hash join to spill to disk while having exactly one row in
# the hash table. Since the hash table checks for out of memory _before_ a row
# is inserted, we will always manage to put one row in the hash table. Thus,
# insert large rows and reduce the join buffer size so that the hash join
# spills to disk when trying to write the second row to the hash table.
CREATE TABLE t1 (col1 TEXT);
INSERT INTO t1 VALUES (REPEAT('A', 50000)), (REPEAT('B', 50000)), (REPEAT('C', 50000)),
(REPEAT('D', 50000)), (REPEAT('E', 50000)), (REPEAT('F', 50000));
ANALYZE TABLE t1;
# Mask out the cost estimates, since they will differ with innodb_page_size.
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT a.col1 FROM t1 AS a, t1 AS b WHERE a.col1 = b.col1;
--disable_result_log
SET join_buffer_size = 128;
SELECT a.col1 FROM t1 AS a, t1 AS b WHERE a.col1 = b.col1;
--enable_result_log
DROP TABLE t1;

--echo # Set up a case where we have very skewed data in the probe input, and we
--echo # degrade into an on-disk hash join. We want to trigger a code path where
--echo # we have empty chunk files from the probe input.
CREATE TABLE t1 (col1 VARCHAR(255));
CREATE TABLE t2 (col1 VARCHAR(255));
INSERT INTO t1 VALUES (SHA2(UUID(), 512));
let $count=9;
while ($count)
{
  eval INSERT INTO t1 SELECT SHA2(UUID(), 512) FROM t1;
  dec $count;
}
INSERT INTO t2 SELECT REPEAT("a", 255) FROM t1;

# Increase the number of pages sampled by InnoDB during ANALYZE TABLE to get
# more stable row count estimates.
SET GLOBAL innodb_stats_persistent_sample_pages = 2000;
ANALYZE TABLE t1, t2;
SET GLOBAL innodb_stats_persistent_sample_pages = DEFAULT;

let $query = SELECT STRAIGHT_JOIN COUNT(*) FROM t1 JOIN t2 ON t1.col1 = t2.col1;

# Mask out the cost estimates, since they will differ with innodb_page_size.
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--disable_result_log
SET join_buffer_size = 1024;
eval $query;
--enable_result_log

DROP TABLE t1, t2;
SET join_buffer_size = DEFAULT;

--echo #
--echo # Bug#29964536 WL#2241: ASSERTION FAILURE IN
--echo #              TEMPTABLE::HANDLER::POSITION() AT SRC/HANDLER.CC
--echo #

CREATE TABLE tc (
  col_int INTEGER,
  col_varchar VARCHAR(1)
);
INSERT INTO tc VALUES (0,'x');

CREATE TABLE tcc (
  col_varchar VARCHAR(1)
);
INSERT INTO tcc VALUES ('r'), ('f'), ('y'), ('u'), ('m'), (NULL);

CREATE TABLE t1 (field1 INTEGER);
INSERT INTO t1 VALUES (0);

# Set up a query where we have hash join of a streamed materialization, which
# verifies that hash join does not try and ask the StreamingIterator for
# row IDs; the StreamingIterator will provide the row IDs automatically.
#
# The output of the query does not matter; it should just not assert.
SET optimizer_switch="firstmatch=off";
UPDATE t1 SET field1 = 9999 WHERE field1 NOT IN ( SELECT alias1.col_int AS
field1 FROM ( tcc, ( SELECT * FROM tc WHERE col_int < 1 ) AS alias1 ) WHERE (
alias1.col_varchar IN ( SELECT col_varchar FROM tcc ) ) GROUP BY field1
HAVING field1 <> 1 );
SET optimizer_switch="firstmatch=on";
DROP TABLE tc,tcc,t1;

--echo # Do a join between DECIMAL and INTEGER to verify that we get a match
--echo # between these two types.
CREATE TABLE t1 (col1 DECIMAL(4, 2));
INSERT INTO t1 VALUES (0);

CREATE TABLE t2 (col1 INTEGER);
INSERT INTO t2 VALUES (0);

ANALYZE TABLE t1, t2;

--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t1 JOIN t2 ON t1.col1 = t2.col1;
SELECT * FROM t1 JOIN t2 ON t1.col1 = t2.col1;
DROP TABLE t1, t2;

--echo # See that we get the correct results with a PAD SPACE collation and
--echo # PAD_CHAR_TO_FULL_LENGTH. Note that the latter is deprecated, so this
--echo # test should go away once the SQL mode is removed.
CREATE TABLE t1 (
  col1 CHAR(4)
) DEFAULT CHARSET=latin1 COLLATE=latin1_general_cs;
INSERT INTO t1 VALUES ("foo");

CREATE TABLE t2 (
  col1 CHAR(40)
) DEFAULT CHARSET=latin1 COLLATE=latin1_general_cs;
INSERT INTO t2 VALUES ("foo");

ANALYZE TABLE t1, t2;

SET sql_mode="PAD_CHAR_TO_FULL_LENGTH";
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t1 JOIN t2 ON t1.col1 = t2.col1;
SELECT * FROM t1 JOIN t2 ON t1.col1 = t2.col1;
SET sql_mode=DEFAULT;
DROP TABLE t1, t2;

--echo # Set up a case where the join planner will set up a BNL with linked
--echo # join buffers, and where the row ID should be kept due to duplicate
--echo # removal. rowid_status will be set on several QEP_TABs to indicate that
--echo # a row ID is needed, even though we should not request the row ID on all
--echo # of them.
CREATE TABLE b1 (col_int INTEGER);

INSERT INTO b1 VALUES (1);

CREATE TABLE c1 (
  col_int INTEGER,
  col_timestamp TIMESTAMP NULL,
  col_decimal DECIMAL(10, 4)
);

INSERT INTO c1 VALUES
  (1741569678,'2004-01-07 20:47:51',-4.7563),
  (-1533615975,'2037-10-27 16:40:24',7.7785);

CREATE TABLE cc1 (
  col_int INTEGER,
  col_decimal DECIMAL(10, 4),
  col_timestamp TIMESTAMP NULL
);

INSERT INTO cc1 VALUES
  (-190646953,6.4052,'2007-11-21 09:45:29'),
  (-423321712,6.9636,'1988-01-04 13:34:47');


SELECT
  1
FROM
  b1
  LEFT JOIN (
      c1
      RIGHT JOIN (SELECT DISTINCT * FROM cc1) AS alias3 ON
          alias3.col_timestamp = c1.col_timestamp
    ) ON b1.col_int = c1.col_int AND 1
WHERE
  EXISTS(
    SELECT
      1
    FROM
      cc1 JOIN c1 ON c1.col_decimal = cc1.col_decimal AND 1
    WHERE
      cc1.col_int <= b1.col_int OR cc1.col_int = c1.col_int
  );
DROP TABLE b1, c1, cc1;

--echo # Yet another problematic case involing duplicate weedout.
CREATE TABLE t1 (
  col_int_key int(11) DEFAULT NULL,
  col_varchar_key varchar(1) DEFAULT NULL,
  col_varchar_nokey varchar(1) DEFAULT NULL,
  KEY col_int_key (col_int_key),
  KEY col_varchar_key (col_varchar_key,col_int_key)
) charset utf8mb4;

INSERT INTO t1 VALUES (4,'v','v');
INSERT INTO t1 VALUES (62,'v','v');
INSERT INTO t1 VALUES (7,'c','c');
INSERT INTO t1 VALUES (1,NULL,NULL);

set optimizer_switch='firstmatch=off';
set optimizer_switch='materialization=off';

--sorted_result
SELECT
  alias1.col_varchar_nokey AS a1_nokey,
  alias1.col_varchar_key AS a1_key,
  alias2.col_varchar_nokey AS a2_nokey
FROM
  t1 AS alias1, t1 AS alias2
WHERE
  (alias1.col_varchar_nokey,alias2.col_varchar_nokey)
  IN
  (
    SELECT
      sq2_alias2.col_varchar_nokey, sq2_alias1.col_varchar_key
    FROM
      t1 AS sq2_alias1, t1 AS sq2_alias2
  )
;
set optimizer_switch=DEFAULT;
DROP TABLE t1;


--echo # A case where we have a hash join iterator both above and below a
--echo # WeedoutIterator.
CREATE TABLE t1(f1 INT(11) NOT NULL);
INSERT INTO t1 VALUES (10);

CREATE TABLE t2
(
  f1 INT(11) NOT NULL AUTO_INCREMENT,
  f2 INT(11) DEFAULT NULL,
  PRIMARY KEY (f1),
  KEY (f2)
);
INSERT INTO t2 VALUES (1, 7), (2, 1), (4, 7), (5, 6), (8, 9), (10, 11);

CREATE TABLE t4(f1 INT DEFAULT NULL);
INSERT INTO t4 VALUES (2);

ANALYZE TABLE t1, t2, t4;

let $query=
SELECT /*+ JOIN_PREFIX(t2@qb2, t4@qb1, ta3, ta4) */
COUNT(*) FROM t1 JOIN t2 AS ta3 JOIN t2 AS ta4
WHERE ta4.f1 IN (SELECT /*+ QB_NAME(qb1) */ f1 FROM t4) AND
      ta3.f2 IN (SELECT /*+ QB_NAME(qb2) */ f2 FROM t2);
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;
eval $query;

DROP TABLE t1, t2, t4;

--echo #
--echo # Bug#30035890 SIG 11 IN HASHJOINITERATOR::READJOINEDROW AT
--echo #              SQL/HASH_JOIN_ITERATOR.CC
--echo #
--echo # Note that this test case needs ASAN to reproduce.
--echo # Note: Different plan with new hypergraph cost model.

CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (7), (7);

CREATE TABLE t2 (b INT, c DATETIME);
INSERT IGNORE INTO t2 VALUES (7, NULL), (7, '2006'), (7, '2002');

--echo # Set up a case where the hash join row buffer will be re-inited.
UPDATE t1
SET a = 42
WHERE a NOT IN (
  SELECT alias2.b FROM t2 AS alias2 JOIN t2 AS alias1 ON (alias2.c = alias1.c)
);

DROP TABLE t1, t2;

--echo #
--echo # Bug#30060691 ASSERTION `M_INDEX_CURSOR.IS_POSITIONED()' IN
--echo #              TEMPTABLE::HANDLER::POSITION()
--echo #

CREATE TABLE c (
  col_int INTEGER,
  col_varchar VARCHAR(1) ,
  col_varchar_key VARCHAR(1));

CREATE TABLE bb (
  pk INTEGER auto_increment,
  col_int_key INTEGER,
  col_varchar VARCHAR(1),
  col_varchar_key VARCHAR(1),
  PRIMARY KEY (pk));

CREATE TABLE cc (
  col_varchar_key VARCHAR(1),
  INDEX idx (col_varchar_key));

INSERT INTO bb VALUES (DEFAULT, 41509313, 'S', 'W');

INSERT INTO c VALUES
  (-792274908, 'P', 'r'),
  (281391051, 'w', 'x'),
  (-1381986093, 'l', '2'),
  (-78303180, 'f', 'Q'),
  (1027797776, 'w', 'G'),
  (-1361294690, 'm', 'L'),
  (65604698, '7', 'Y'),
  (-964881813, 'j', 'F'),
  (1831120981, 'q', 'q'),
  (-573388832, 'F', 'M'),
  (571640392, '1', 'R'),
  (857813414, 'y', 'l'),
  (555892383, 'x', 'P'),
  (601556555, 'z', 'k'),
  (-578249624, 'N', 'e'),
  (-843749952, '4', 'J'),
  (2058477272, '4', 'R'),
  (-1732353317, 'C', 'Z'),
  (-1639317818, '9', 'f'),
  (19700948, 'K', 'V');

INSERT INTO cc VALUES
  ('b'), ('E'), ('v'), ('4'), ('L'), ('g'), ('i'), ('D'), ('S'), ('s'), ('4'),
  ('5'), ('4'), ('y'), ('v'), ('Z'), ('O'), ('2'), ('v'), ('5');

ANALYZE TABLE c, bb, cc;

let $query = SELECT *
FROM
  cc AS alias1
  LEFT JOIN (
      (
        bb AS alias2
        INNER JOIN (SELECT DISTINCT sq1_alias1.* FROM bb AS sq1_alias1)
            AS alias3 ON alias3.col_int_key = alias2.col_int_key
      )
    ) ON alias3.col_varchar_key = alias2.col_varchar_key
WHERE
  alias1.col_varchar_key
  IN (
      SELECT
        sq2_alias1.col_varchar AS sq2_field1
      FROM
        c AS sq2_alias1
      WHERE
        sq2_alias1.col_varchar_key != alias2.col_varchar
        AND sq2_alias1.col_int > alias2.pk
    );

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--echo # We only want to see that the query does not hit an assertion, so ignore
--echo # the results.
--disable_result_log
eval $query;
--enable_result_log

DROP TABLE bb, c, cc;

--echo #
--echo # Bug#30049217 ASSERTION FAILURE AT
--echo #              TEMPTABLE::HANDLER::POSITION|SRC/HANDLER.CC
--echo #
CREATE TABLE t1 (c1 INT);
INSERT INTO t1 VALUES
(6),(7),(8),(9),(10),(11),(12),(13),(14),(15),(16),(17),(18),(19),(20);

CREATE TABLE t2 (c2 INT, c3 INT, KEY (c3));
INSERT INTO t2 VALUES
(1,-823867270),
(19,1130654803),
(20,1299270309);

CREATE TABLE t3 (c4 INT);
INSERT INTO t3 VALUES (1);

ANALYZE TABLE t1, t2, t3;

SELECT *
FROM ((SELECT DISTINCT * FROM t2) AS alias2 JOIN t3 ON (t3.c4 = alias2.c2))
WHERE (EXISTS (SELECT * FROM (t1 LEFT JOIN (t3 JOIN t2 ON (t2.c3 = t3.c4)) ON
(1))))
AND alias2.c3 < 19;

DROP TABLE t1, t2, t3;

--echo #
--echo # Bug#30153695 ASSERTION SIG6 TEMPTABLE::HANDLER::POSITION
--echo #              SRC/HANDLER.CC:715
--echo #

CREATE TABLE c (
  col_date date,
  col_datetime_key datetime,
  col_varchar_key varchar (1),
  col_varchar varchar (1),
  col_date_key date,
  col_int_key int,
  col_time time,
  col_time_key time,
  col_int int,
  pk integer auto_increment,
  col_datetime datetime,
  key (col_datetime_key ),
  key (col_varchar_key ),
  key (col_date_key ),
  key (col_int_key ),
  key (col_time_key ),
  primary key (pk)) ENGINE=innodb;

INSERT IGNORE INTO c VALUES
  ('2001-07-23', '2004-12-11', 'k', 's', NULL, 7, '2004-11-12', '2000-03-18', 3,
   NULL, NULL),
  (NULL, NULL, 's', 'j', NULL, 6, NULL, '2005', 1, NULL, NULL),
  ('2006-07-02', NULL, 'w', 'y', NULL, 2, '04:35:59.017853', '2002', 7, NULL,
   '2004-09-04 21:23:05.023144'),
  (NULL, '2009-02-16 21:37:23.010045', 'w', 'o', '2005-05-25', NULL, NULL,
   '04:32:06.000870', 9, NULL, '2004'),
  (NULL, NULL, 'y', 'k', '2002-12-15', 81, NULL, '2009-03-14', 3, NULL, NULL),
  (NULL, '2005', 'x', 's', '2004-07-12', 9, NULL, NULL, 7, NULL, '2009'),
  ('2003', '2000-11-08', 'd', 'h', '2002-09-25', 8, NULL, '2002', NULL, NULL,
   '2004'),
  ('2000', '2008-01-08 20:49:13.011386', 't', 'w', '2000-12-11', 6,
   '18:31:35.007025', '19:28:20.040544', 4, NULL, '2005-03-13'),
  ('2006-10-04', '2000-12-16', 'i', 'f', NULL, 3, '2008', NULL, 5, NULL,
   '2003-12-03 13:55:06.040156'),
  ('2009-07-26', '2009-11-22 07:59:12.037926', 'o', 'n', '2004-07-23', 4,
   '2005', '12:00:51.020344', 5, NULL, '2006'),
  ('2009-02-25', NULL, 'm', NULL, '2003', NULL, '2000', '2002-07-28', 1, NULL,
   '2004-06-26'),
  ('2008-01-11', '2001-05-27', 'c', 'w', '2001-11-21', 4, '2004-07-23',
   '2005-07-19', 3, NULL, '2001'),
  ('2009', NULL, 'x', NULL, NULL, 6, '2006-10-03', NULL, 1, NULL, '2009-12-03'),
  ('2008-09-22', '2008-08-09 11:16:52.037869', 'r', 'c', '2008-01-23', 3, NULL,
   NULL, 6, NULL, '2008'),
  ('2007-01-21', NULL, 'u', 'u', '2008', 5, '2003-07-15', '07:04:43.054922',
   NULL, NULL, NULL),
  ('2009-06-15', '2004-01-25', 'x', NULL, NULL, 189, '2008', '2000-06-14', 1,
   NULL, NULL),
  ('2005', '2008-03-22', NULL, 'g', '2008', 1, '20:53:08.022885', '2006', 3,
   NULL, '2009-04-06 15:24:52.051014'),
  ('2002', '2003-07-10 12:29:23.023649', 'g', 'u', '2000-10-16', 9, '2003',
   '2006', 9, NULL, NULL),
  ('2005-10-23', NULL, 's', 'x', '2005', 9, '2008-07-09', '2001-08-12', 8, NULL,
   NULL),
  ('2005', NULL, 'g', 'm', '2000-01-03', 9, '2008', NULL, 1, NULL,
   '2001-01-21');

CREATE TABLE cc (
  col_date date,
  col_int int,
  col_int_key int,
  col_varchar_key varchar (1),
  col_datetime_key datetime,
  col_datetime datetime,
  pk integer auto_increment,
  col_varchar varchar (1),
  col_time_key time,
  col_time time,
  col_date_key date,
  key (col_int_key ),
  key (col_varchar_key ),
  key (col_datetime_key ),
  primary key (pk),
  key (col_time_key ),
  key (col_date_key )) ENGINE=innodb;

ALTER TABLE cc DISABLE KEYS;
INSERT IGNORE  INTO cc VALUES
  ('2006-06-04', 3, 0, 'y', '2006-04-12 00:44:48.055959', NULL, NULL, 'l',
   '2005-01-10', '2004', '2004-07-14'),
  ('2008', 6, 8, NULL, '2006-10-23', NULL, NULL, 'a', NULL, NULL, '2000-04-26'),
  ('2009-06-11', NULL, 9, 'w', '2008', '2005', NULL, 'q', '04:42:05.061538',
   '2004-08-18', NULL),
  ('2007-03-01', 4, 7, 'f', NULL, '2000-10-06 15:26:40.040137', NULL, 'd',
   '2008', '2006-11-17', '2006'),
  ('2001-02-08', 4, 210, 'j', '2003-11-14 04:26:34.047333', NULL, NULL, 'h',
   '06:13:13.012974', '02:20:21.050151', '2006-08-20'),
  ('2000', 9, 5, 'b', '2006-12-16', NULL, NULL, 'z', '2000-09-09', '2007-06-15',
   '2008'),
  (NULL, 1, 6, 'z', '2007-12-10 00:57:04.007939', NULL, NULL, 'i', '2002-02-11',
   '2004', '2006-08-08'),
  ('2007', NULL, 1, 'w', '2007-09-03 21:11:14.028959', '2009', NULL, 'n',
   '2009-05-03', '2005-06-23', NULL),
  (NULL, 4, NULL, 'f', '2007-04-12', NULL, NULL, 'f', '2007-12-01', '2006',
   '2000-05-11'),
  ('2008', 7, 1, 's', NULL, NULL, NULL, 'o', '2002', '2003', '2009-12-03'),
  (NULL, 5, 62, 'i', '2009-10-06 12:22:10.055548', '2003', NULL, 'p', NULL,
   NULL, '2006-02-03'),
  ('2006-02-10', 4, 9, 'g', NULL, '2000-07-26 23:20:24.031805', NULL, 'c',
   '2007-12-12', '2002', '2003'),
  ('2000', 5, 0, 'j', '2000-02-23', '2000', NULL, 'a', '2005', '2000-04-15',
   '2000-09-19'),
  (NULL, 2, 9, 'q', '2003-12-24', NULL, NULL, NULL, NULL, '2000', '2008-05-23'),
  (NULL, 9, NULL, 'i', '2003-10-22 02:03:47.003490', '2006-01-03', NULL, 'b',
   NULL, '2003', '2008-01-21'),
  ('2008-06-09', 9, 0, 'a', '2000', NULL, NULL, 'c', '21:15:46.049912', '2001',
   NULL),
  ('2000', 2, 8, NULL, '2009-11-27', NULL, NULL, NULL, '2004-05-08',
   '12:30:30.041709', '2005-12-01'),
  ('2009-03-27', 3, 0, 'l', '2009', '2009', NULL, 'a', NULL, '04:16:53.049190',
   NULL),
  ('2008-08-26', 114, 3, 'o', '2008-03-06', NULL, NULL, 'k', '07:26:47.018798',
   '2002-08-17', '2004-09-07'),
  (NULL, 8, 7, 'm', '2007-12-28 23:49:04.022501', '2005-04-08', NULL, 't',
   '2000-11-12', '22:19:29.060590', '2005-09-20');
ALTER TABLE cc ENABLE KEYS;

ANALYZE TABLE c, cc;

let $query = SELECT
  alias1.pk AS field1
FROM
  (
    SELECT
      sq1_alias2.*
    FROM
      cc AS sq1_alias1
      RIGHT JOIN cc AS sq1_alias2 ON
          sq1_alias2.col_varchar_key = sq1_alias1.col_varchar_key
    LIMIT
      100
  )
    AS alias1
WHERE
  alias1.col_varchar_key
  IN (
      SELECT
        sq2_alias1.col_varchar_key AS sq2_field1
      FROM
        (cc AS sq2_alias1, c AS sq2_alias2)
      WHERE
        sq2_alias1.col_varchar_key != alias1.col_varchar
    )
GROUP BY
  field1
HAVING
  field1 != 'pg'
ORDER BY
  alias1.col_int_key DESC, field1
LIMIT 2 OFFSET 2;

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

DROP TABLE c, cc;


--echo #
--echo # Bug#30119783 SIG11 IN
--echo #   HASH_JOIN_BUFFER::STOREFROMTABLEBUFFERS|SQL/HASH_JOIN_BUFFER.CC
--echo #
CREATE TABLE b(pk INT PRIMARY KEY, col_varchar VARCHAR(1));
CREATE TABLE cc(pk INT PRIMARY KEY, col_varchar VARCHAR(1));
INSERT INTO b VALUES (1, '4');
INSERT INTO cc VALUES (1, 'c'), (2, 'c');
ANALYZE TABLE b, cc;

# A complicated test case, but what we want to achieve is a weedout over a
# hash join, where the hash join build input is a combination of left join over
# a temporary table. Very simplified, it will look something like this:
# -> Weedout
#   -> Inner hash join
#     -> Table scan
#     -> Hash
#       -> Nested loop left join
#         -> Table scan
#         -> Materialize
#           -> Table scan
#
# The behavior we want to trigger is that hash join should ask for the row ID
# from a temporary table, where the temporary table row is NULL-complemented.

let $query = SELECT
  table1.col_varchar
FROM
  (
    SELECT
      subquery1_t1.*
    FROM
      b AS subquery1_t1
      INNER JOIN cc AS subquery1_t2 ON
          subquery1_t1.col_varchar = subquery1_t2.col_varchar
  )
    AS table1
  LEFT JOIN (
      SELECT
        col_varchar
      FROM
        cc AS subquery2_t1
      GROUP BY
        subquery2_t1.col_varchar
    )
      AS table2 ON
      table2.col_varchar = table1.col_varchar
      AND table1.col_varchar
        IN (
            SELECT
              lower(subquery3_t1.pk) AS subquery3_field1
            FROM
              b AS subquery3_t1
          );

--echo # Note: Different plan with new hypergraph cost model.
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

DROP TABLE b, cc;

--echo #
--echo # Bug#30049083 [REGRESSION]REPLACE/INSERT WITH LIMIT TAKING MORE TIME AND
--echo #              SPACE
--echo #
--echo # If the query has a LIMIT, the hash join should not spill to disk. Note
--echo # that if the query contains either grouping or sorting, we allow spill
--echo # to disk even if the query contains a LIMIT.

CREATE TABLE t1 (col1 BIGINT);
INSERT INTO t1 SELECT 1;
INSERT INTO t1 SELECT col1 + 1 FROM t1;
INSERT INTO t1 SELECT col1 + 2 FROM t1;
INSERT INTO t1 SELECT col1 + 4 FROM t1;
INSERT INTO t1 SELECT col1 + 8 FROM t1;
INSERT INTO t1 SELECT col1 + 16 FROM t1;
INSERT INTO t1 SELECT col1 + 32 FROM t1;
INSERT INTO t1 SELECT col1 + 64 FROM t1;
INSERT INTO t1 SELECT col1 + 128 FROM t1;
INSERT INTO t1 SELECT col1 + 256 FROM t1;
INSERT INTO t1 SELECT col1 + 512 FROM t1;

CREATE TABLE t2 SELECT col1 FROM t1;

ANALYZE TABLE t1, t2;

SET join_buffer_size = 2048;  # 2 kB.

let $hash_join_file_operations =
  SELECT COUNT_STAR > 0 FROM performance_schema.file_summary_by_event_name
    WHERE event_name LIKE '%hash_join%';

--echo # This should spill to disk since we do not have any LIMIT.
--disable_result_log
TRUNCATE performance_schema.file_summary_by_event_name;
SELECT * FROM t1, t2 WHERE t1.col1 = t2.col1;
--enable_result_log
eval $hash_join_file_operations;

--echo # This should NOT spill to disk since we have a LIMIT.
--disable_result_log
TRUNCATE performance_schema.file_summary_by_event_name;
SELECT * FROM t1, t2 WHERE t1.col1 = t2.col1 LIMIT 1;
--enable_result_log
eval $hash_join_file_operations;

--echo # This should spill to disk since we have sorting.
TRUNCATE performance_schema.file_summary_by_event_name;
SELECT * FROM t1, t2 WHERE t1.col1 = t2.col1 ORDER BY t1.col1 LIMIT 1;
eval $hash_join_file_operations;

--echo # This should spill to disk since we have (implicit) grouping.
TRUNCATE performance_schema.file_summary_by_event_name;
SELECT SUM(t1.col1) FROM t1, t2 WHERE t1.col1 = t2.col1 LIMIT 10;
eval $hash_join_file_operations;

SET join_buffer_size = DEFAULT;
DROP TABLE t1,t2;

--echo #
--echo # Bug#30214767 SIG11 AT QUICK_INDEX_MERGE_SELECT::GET_NEXT |
--echo #              SQL/OPT_RANGE.CC
--echo #
--echo # Set up a query with hash join, where the build input uses an index
--echo # range scan with index merge sort-union. Also, a LIMIT greater than
--echo # the number of rows satisfying the join condition is needed to
--echo # reproduce the bug. What we want to achieve is to get the hash join
--echo # to call Read() on the build input after it has returned EOF. This can
--echo # be triggered by using LIMIT, as this causes the hash join to go back
--echo # and read from the build input after the probe iterator has returned
--echo # EOF (see comment on HashJoinIterator regarding spill to disk and LIMIT
--echo # for more details around this).
CREATE TABLE t1 (col1 INTEGER);
CREATE TABLE t2 (
  col1 INTEGER,
  col2 INTEGER,
  col3 INTEGER,
  INDEX idx_a (col2),
  INDEX idx_b (col3));

INSERT INTO t1 VALUES (1);
INSERT INTO t2 VALUES (1, 1, 1);
ANALYZE TABLE t1, t2;

let $query = SELECT /*+ JOIN_ORDER(t2, t1) INDEX_MERGE(t2) */ t1.col1
  FROM t1
  JOIN t2 ON t1.col1 = t2.col1
  WHERE t2.col2 > 0 OR t2.col3 > 0 LIMIT 10;

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;
DROP TABLE t1, t2;


--echo #
--echo # Bug#30224582 ASSERTION `M_INDEX_CURSOR.IS_POSITIONED()' FAILED
--echo #
--echo # Set up a query where the hash join build input consists of a
--echo # materialized table, where we do an index lookup on the materialized
--echo # table. The LIMIT is also needed in order to trigger a second build
--echo # phase in the hash join.
CREATE TABLE t1 (col1 INTEGER);
CREATE TABLE t2 (col1 INTEGER);
INSERT INTO t1 VALUES (1);
INSERT INTO t2 VALUES (1);
ANALYZE TABLE t1, t2;

let $query = SELECT /*+ JOIN_ORDER(table1, t2) */
  *
FROM
  (
    SELECT
      DISTINCT t1.*
    FROM
      t1
  ) AS table1 JOIN t2
  WHERE table1.col1 = 1
LIMIT 50;

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;
DROP TABLE t1, t2;

--echo #
--echo # Bug#30306279 ASAN: MEMCPY-PARAM-OVERLAP NEAR
--echo #              EXTRACT_VALUE_FOR_HASH_JOIN
--echo #
--echo # Set up a test where geometry data is written to chunk files on disk.
--echo # Note that the test only fails with ASAN.
CREATE TABLE t1 (col1 GEOMETRY);
INSERT INTO t1 SELECT ST_GeomFromText(CONCAT('GEOMETRYCOLLECTION(POINT(1 1)',
                                      REPEAT(',POINT(1 1)', 100), ')'));
INSERT INTO t1 SELECT ST_GeomFromText(CONCAT('GEOMETRYCOLLECTION(POINT(1 1)',
                                      REPEAT(',POINT(1 1)', 100), ')')) FROM t1;
INSERT INTO t1 SELECT ST_GeomFromText(CONCAT('GEOMETRYCOLLECTION(POINT(1 1)',
                                      REPEAT(',POINT(1 1)', 100), ')')) FROM t1;

SET join_buffer_size = 128;
--disable_result_log
SELECT * FROM t1 a JOIN t1 b ON a.col1 = b.col1;
--enable_result_log
SET join_buffer_size = DEFAULT;
DROP TABLE t1;

--echo #
--echo # Bug#30535541 HASH JOIN DOES NOT HANDLE PAD SPACE COLLATIONS CORRECTLY
--echo #
--echo # Test that hash join compares strings that only differs in end spaces
--echo # equally in PAD SPACE collations.
CREATE TABLE t1 (c VARCHAR(255) COLLATE utf8mb4_bin);
CREATE TABLE t2 (c VARCHAR(255) COLLATE utf8mb4_bin);
INSERT INTO t1 VALUES ('foo');
INSERT INTO t2 VALUES ('foo ');
ANALYZE TABLE t1, t2;

let $query = SELECT * FROM t1 JOIN t2 ON t1.c = t2.c;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;
DROP TABLE t1, t2;

--echo #
--echo # Bug#30579922 INCORRECT RESULT IF HASH-JOIN ALGORITHM EXHAUST ITS
--echo #              JOIN_BUFFER
--echo #
--echo # Set up a query where the build input is a nested loop, where the inner
--echo # side of the nested loop also has a filter. We want to see that if the
--echo # hash join is not allowed to spill to disk, it will restore the last row
--echo # read from the build iterator before doing a re-fill of the hash table.
CREATE TABLE t1 (i BIGINT);
INSERT INTO t1 SELECT 1;
INSERT INTO t1 SELECT i + POW(2, 0) FROM t1;
INSERT INTO t1 SELECT i + POW(2, 1) FROM t1;
INSERT INTO t1 SELECT i + POW(2, 2) FROM t1;
INSERT INTO t1 SELECT i + POW(2, 3) FROM t1;
INSERT INTO t1 SELECT i + POW(2, 4) FROM t1;
INSERT INTO t1 SELECT i + POW(2, 5) FROM t1;
CREATE TABLE t2 AS SELECT i FROM t1;
CREATE TABLE t3 AS SELECT i + 60 AS i FROM t1;

ANALYZE TABLE t1, t2, t3;

let $query =
  SELECT /*+ NO_BNL(t2) JOIN_ORDER(t1, t2, t3) */ * FROM t1
    JOIN t2 ON t1.i > t2.i
    JOIN t3 ON t2.i = t3.i;

SET join_buffer_size = 128;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;

--sorted_result
eval $query;

--sorted_result
eval $query LIMIT 1000;

SET join_buffer_size = DEFAULT;
DROP TABLE t1, t2, t3;

--echo #
--echo #  Bug#30282693 SIG6 IN TEMPTABLE::HANDLER::POSITION() AT SRC/HANDLER.CC
--echo #
--echo # Create a query where we get a confluent weedout as the right side of an
--echo # antijoin. Since a confluent weedout returns at most one row, the
--echo # iterator executor will replace it with a LIMIT 1. Note that the test
--echo # case is not easy to understand, but it is the smallest test case I
--echo # could make that reproduces the bug.
CREATE TABLE t1 (
  col1 INTEGER,
  KEY idx (col1)
);

CREATE TABLE t2 (
  col1 INTEGER,
  KEY idx (col1)
);

INSERT INTO t2 VALUES
  (1),(2),(3),(4),(5),(6),(7),(8),(9),(10),(11),(12),(13),(14),(15),(16),(17);

ANALYZE TABLE t1, t2;

let $query = SELECT /*+ JOIN_ORDER(const_table, t2, t1, t1_2) */ 1 FROM t2
  RIGHT OUTER JOIN (VALUES ROW(4), ROW(5)) AS const_table ON TRUE
  WHERE NOT EXISTS (
    SELECT t1.col1 FROM t1
    INNER JOIN t1 AS t1_2 ON t1_2.col1 = t1.col1
    WHERE t1.col1 <> t2.col1
  ) AND t2.col1 < 49;

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

DROP TABLE t1, t2;

--echo #
--echo # Bug#30626264 ASSERTS IN BASIC_ROW_ITERATORS.H:345:
--echo #              FAKESINGLEROWITERATOR::SETNULLROWFLAG
--echo #

CREATE TABLE t1 (
  pk INTEGER
);
INSERT INTO t1 VALUES (1), (2);

CREATE TABLE t2 (
  pk INTEGER NOT NULL,
  PRIMARY KEY (pk)
);

INSERT INTO t2 VALUES (9);

CREATE TABLE t3 (
  pk INTEGER
);

ANALYZE TABLE t1, t2, t3;

# Set up a hash join where we get the const tables on the right-hand side,
# and that is run multiple times (so that the hash table is built twice).
# We don't really need to join in t3 twice, except the optimizer doesn't
# select a hash join otherwise.
let $query =
  SELECT (
    SELECT t1.pk
    FROM t2
      LEFT JOIN t3 USING (pk)
      JOIN t3 AS t4 USING (pk)
      WHERE t2.pk = 9
  ) FROM t1;
--replace_regex $elide_costs
eval explain format=tree $query;
eval $query;

DROP TABLE t1, t2, t3;
--echo # See that non equi-join BNL are executed using hash join.
CREATE TABLE t1 (col1 INT);
CREATE TABLE t2 (col1 INT);
ANALYZE TABLE t1, t2;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t1 JOIN t2 ON t1.col1 < t2.col1;
DROP TABLE t1, t2;

--echo # Semijoin.
CREATE TABLE t1 (i INTEGER);
CREATE TABLE t2 (i INTEGER);
INSERT INTO t1 VALUES (1), (2);
INSERT INTO t2 VALUES (1), (1);
ANALYZE TABLE t1, t2;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t1 WHERE (t1.i) IN (SELECT t2.i FROM t2);
SELECT * FROM t1 WHERE (t1.i) IN (SELECT t2.i FROM t2);
DROP TABLE t1, t2;

CREATE TABLE t1 (col1 INTEGER, col2 INTEGER);
CREATE TABLE t2 (col1 INTEGER, col2 INTEGER);
INSERT INTO t1 VALUES (1, 1), (2, 2), (2, 2), (2, 2), (2, 2), (2, 2);
INSERT INTO t2 VALUES (1, 1), (1, 2), (3, 3), (3, 3), (3, 3), (3, 3);
ANALYZE TABLE t1, t2;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree
  SELECT * FROM t1 WHERE (t1.col1) IN
    (SELECT t2.col1 FROM t2 WHERE t2.col2 < 3);
DROP TABLE t1, t2;

--echo # Test a query where the optimizer will create a semijoin condition that
--echo # is not an equi-join condition. These kind of conditions must be
--echo # attached as "extra" conditions in the hash join iterator in order to
--echo # produce correct results.
CREATE TABLE t1 (col1 INTEGER);
CREATE TABLE t2 (col1 INTEGER);
INSERT INTO t1 VALUES (1), (3), (4), (5);
INSERT INTO t2 VALUES (2), (3), (4), (6);
ANALYZE TABLE t1, t2;

let $query = SELECT * FROM t1 WHERE 1 IN (SELECT t1.col1 = t2.col1 FROM t2);
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;
DROP TABLE t1, t2;

--echo # Exercise a test where we get multiple tables in the probe input.
CREATE TABLE t1(a INTEGER);
CREATE TABLE t2(a INTEGER);
CREATE TABLE t3(a INTEGER);

INSERT INTO t1 VALUES (0);
INSERT INTO t2 VALUES (0);
INSERT INTO t3 VALUES (0);

ANALYZE TABLE t1, t2, t3;

SET optimizer_switch="materialization=off,duplicateweedout=off";
let $query = SELECT * FROM t1 LEFT JOIN t2 ON t1.a=t2.a
  WHERE (t1.a,t2.a) IN (SELECT a, a FROM t3);

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;
SET optimizer_switch=DEFAULT;
DROP TABLE t1, t2, t3;

--echo # See that semijoin with spill to disk works as expected.
CREATE TABLE t1 (col1 BIGINT, col2 BIGINT);
INSERT INTO t1 SELECT POW(2, 0), POW(2, 0);
let $count=0;
while ($count < 10)
{
  eval INSERT INTO t1
         SELECT col1 + POW(2, $count), col2 + POW(2, $count) FROM t1;
  inc $count;
}
CREATE TABLE t2 AS SELECT * FROM t1;
UPDATE t1 SET col2 = col1 + 1 WHERE MOD(col1, 2) = 0;

ANALYZE TABLE t1, t2;

let $query_no_extra_conditions =
  SELECT SUM(t1.col1), SUM(t1.col2) FROM t1 WHERE t1.col1 IN
    (SELECT t2.col1 FROM t2);
let $query_with_extra_conditions =
  SELECT SUM(t1.col1), SUM(t1.col2) FROM t1 WHERE t1.col1 IN
    (SELECT t2.col1 FROM t2 WHERE t1.col2 <> t2.col2);

SET join_buffer_size = 1024;
SET optimizer_switch = "materialization=off";

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query_no_extra_conditions;
eval $query_no_extra_conditions;

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query_with_extra_conditions;
eval $query_with_extra_conditions;

SET optimizer_switch = DEFAULT;
SET join_buffer_size = DEFAULT;
DROP TABLE t1, t2;

--echo # Set up a test case where hash join spills to disk, and the chunk files
--echo # for the build input is too big to fit into memory. This causes the
--echo # probe chunk file to be read multiple times. What we want to see is that
--echo # if a row from the probe chunk file finds a match in the first part of
--echo # the build chunk file, the row must not be sent to the client the second
--echo # time the row is read from the probe chunk. Note that it is not trivial
--echo # to set up a test case that is guaranteed to hit this case.
CREATE TABLE build (col1 BIGINT);
CREATE TABLE probe (col1 BIGINT);
INSERT INTO probe SELECT 1;
INSERT INTO probe SELECT col1 + POW(2, 0) FROM probe;
INSERT INTO probe SELECT col1 + POW(2, 1) FROM probe;
INSERT INTO probe SELECT col1 + POW(2, 2) FROM probe;
INSERT INTO probe SELECT col1 + POW(2, 3) FROM probe;
INSERT INTO probe SELECT col1 + POW(2, 4) FROM probe;
INSERT INTO probe SELECT col1 + POW(2, 5) FROM probe;
INSERT INTO probe SELECT col1 + POW(2, 6) FROM probe;
INSERT INTO probe SELECT col1 + POW(2, 7) FROM probe;
INSERT INTO build SELECT col1 FROM probe;
INSERT INTO build SELECT col1 * 2 FROM probe;
INSERT INTO build SELECT col1 * 3 FROM probe;
ANALYZE TABLE build, probe;

let $query = SELECT COUNT(*) FROM probe WHERE col1 IN (SELECT col1 FROM build);

SELECT COUNT(*) AS expected_num_rows FROM probe;
SET join_buffer_size = 128;
SET optimizer_switch="materialization=off";

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

SET join_buffer_size = DEFAULT;
SET optimizer_switch = DEFAULT;
DROP TABLE build, probe;


--echo # Using LIMIT in the query rejects hash join from spilling to disk,
--echo # causing the probe input to be read multiple times if the build input
--echo # does not fit in memory. This test case is somewhat similar to the
--echo # above; see that we do not output rows from the probe input multiple
--echo # times even if the probe input is read multiple times.
CREATE TABLE build (col1 BIGINT);
CREATE TABLE probe (col1 BIGINT);
INSERT INTO probe SELECT 1;
INSERT INTO probe SELECT col1 + POW(2, 0) FROM probe;
INSERT INTO probe SELECT col1 + POW(2, 1) FROM probe;
INSERT INTO probe SELECT col1 + POW(2, 2) FROM probe;
INSERT INTO probe SELECT col1 + POW(2, 3) FROM probe;
INSERT INTO probe SELECT col1 + POW(2, 4) FROM probe;
INSERT INTO probe SELECT col1 + POW(2, 5) FROM probe;
INSERT INTO probe SELECT col1 + POW(2, 6) FROM probe;
INSERT INTO probe SELECT col1 + POW(2, 7) FROM probe;
INSERT INTO build SELECT col1 FROM probe;
INSERT INTO build SELECT col1 FROM probe ORDER BY col1 DESC;
ANALYZE TABLE build, probe;

SET optimizer_switch = "materialization=off";
SET join_buffer_size = 1024;

# Note that we cannot use ORDER BY in the query, as that will allow hash join to
# spill to disk even if LIMIT is present. Instead, we use --sorted_result to get
# a deterministic output.
let $query = SELECT * FROM probe WHERE col1 IN (SELECT * FROM build);

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;

--sorted_result
eval $query;

--sorted_result
eval $query LIMIT 1000;

SET optimizer_switch = DEFAULT;
SET join_buffer_size = DEFAULT;
DROP TABLE build, probe;

--echo #
--echo # Bug#30512118 WL#13476: ASSERTION FAILURE AT
--echo #              HASHJOINITERATOR::INITPROBEITERATOR()
--echo #
--echo # Execute a query that causes the hash join to do everything in memory
--echo # with multiple refills of the hash table. This can be triggered when the
--echo # build input does not fit in memory, and the iterator is not allowed to
--echo # spill to disk due to LIMIT in the query.
CREATE TABLE t1 (col1 VARCHAR(255));
INSERT INTO t1 SELECT UUID();
INSERT INTO t1 SELECT UUID() FROM t1;
INSERT INTO t1 SELECT UUID() FROM t1;
INSERT INTO t1 SELECT UUID() FROM t1;
INSERT INTO t1 SELECT UUID() FROM t1;
INSERT INTO t1 SELECT UUID() FROM t1;
INSERT INTO t1 SELECT UUID() FROM t1;
INSERT INTO t1 SELECT UUID() FROM t1;
CREATE TABLE t2 AS SELECT UUID() AS col1 FROM t1;

ANALYZE TABLE t1, t2;

SET join_buffer_size = 128;
let $query = SELECT * FROM t1 JOIN t2 ON t1.col1 = t2.col1 LIMIT 1000;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;

# The results are not interesting. We only want to see that we do not assert.
--disable_result_log
eval $query;
--enable_result_log

SET join_buffer_size = DEFAULT;
DROP TABLE t1, t2;

--echo # Antijoin.
CREATE TABLE t1 (f1 INTEGER PRIMARY KEY);
INSERT INTO t1 VALUES (1), (2), (3);

CREATE TABLE t2 (f1 INTEGER NOT NULL);
INSERT INTO t2 VALUES (1), (2), (3), (4);
ANALYZE TABLE t1, t2;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t1 LEFT JOIN t2 USING (f1) WHERE t2.f1 IS NULL;
SELECT * FROM t1 LEFT JOIN t2 USING (f1) WHERE t2.f1 IS NULL;
DROP TABLE t1,t2;

--echo # Set up a case where we get multiple antijoin conditions AND'ed together
--echo # inside a is_not_null_compl. We want to see that the server is able to
--echo # split these conditions and attach them as antijoin conditions to the
--echo # hash join.
CREATE TABLE t1 (
  aid INTEGER NOT NULL,
  bid INTEGER NOT NULL);
CREATE TABLE t2 LIKE t1;

INSERT INTO t1 VALUES (1,1),(1,2),(2,1),(2,2);
INSERT INTO t2 VALUES (1,2),(2,2);

ANALYZE TABLE t1, t2;

let $query = SELECT * FROM t1
  WHERE t1.aid NOT IN (SELECT aid FROM t2 WHERE bid = t1.bid);

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;
DROP TABLE t1, t2;

--echo # Check that hash antijoin can handle NULL values correctly.
CREATE TABLE t1 (col1 INTEGER);
CREATE TABLE t2 (col1 INTEGER);
INSERT INTO t1 VALUES (NULL);
ANALYZE TABLE t1, t2;
let $query = SELECT * FROM t1
  WHERE 1 IN (SELECT 1 FROM t2 WHERE t1.col1 = t2.col1) IS FALSE;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;
DROP TABLE t1, t2;

--echo # See that antijoin with spill to disk works.
CREATE TABLE t1 (col1 BIGINT NOT NULL, col2 BIGINT NOT NULL);
INSERT INTO t1 SELECT POW(2, 0), POW(2, 0);
let $count=0;
while ($count < 10)
{
  eval INSERT INTO t1
         SELECT col1 + POW(2, $count), col2 + POW(2, $count) FROM t1;
  inc $count;
}
CREATE TABLE t2 AS SELECT * FROM t1;
UPDATE t1 SET col2 = col1 + 1 WHERE MOD(col1, 2) = 0;

ANALYZE TABLE t1, t2;

let $query_no_extra_conditions =
  SELECT SUM(t1.col1), SUM(t1.col2) FROM t1 WHERE t1.col1 NOT IN
    (SELECT t2.col1 FROM t2);
let $query_with_extra_conditions =
  SELECT SUM(t1.col1), SUM(t1.col2) FROM t1 WHERE t1.col1 NOT IN
    (SELECT t2.col1 FROM t2 WHERE t1.col2 < t2.col2);

SET join_buffer_size = 1024;
SET optimizer_switch = "materialization=off";

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query_no_extra_conditions;
eval $query_no_extra_conditions;

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query_with_extra_conditions;
eval $query_with_extra_conditions;

SET optimizer_switch = DEFAULT;
SET join_buffer_size = DEFAULT;
DROP TABLE t1, t2;

--echo # Set up an antijoin that spills to disk, and the probe chunk files are
--echo # too big to fit in memory. This causes the hash join to read the probe
--echo # chunk files in several smaller chunks, which in turn enables probe row
--echo # saving. So this test tries to verify that on-disk antijoin with probe
--echo # row saving works correctly. This is not the easiest scenario to set up
--echo # correctly, and it is difficult to show that on-disk antijoin with probe
--echo # row saving actually is triggered...
CREATE TABLE build (col1 BIGINT NOT NULL);
CREATE TABLE probe (col1 BIGINT NOT NULL);
INSERT INTO probe SELECT 1;
INSERT INTO probe SELECT col1 + POW(2, 0) FROM probe;
INSERT INTO probe SELECT col1 + POW(2, 1) FROM probe;
INSERT INTO probe SELECT col1 + POW(2, 2) FROM probe;
INSERT INTO probe SELECT col1 + POW(2, 3) FROM probe;
INSERT INTO probe SELECT col1 + POW(2, 4) FROM probe;
INSERT INTO probe SELECT col1 + POW(2, 5) FROM probe;
INSERT INTO probe SELECT col1 + POW(2, 6) FROM probe;
INSERT INTO probe SELECT col1 + POW(2, 7) FROM probe;
INSERT INTO build SELECT col1 FROM probe;
INSERT INTO build SELECT col1 * 2 FROM probe;
INSERT INTO build SELECT col1 * 3 FROM probe;

# This value should be the only one returned from the antijoin below.
DELETE FROM build WHERE col1 = 254;

ANALYZE TABLE build, probe;

let $query = SELECT * FROM probe WHERE col1 NOT IN (SELECT col1 FROM build);

SET join_buffer_size = 128;
SET optimizer_switch="materialization=off";

# Mask out the cost estimates, since they will differ by how much of the data is in RAM.
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

SET join_buffer_size = DEFAULT;
SET optimizer_switch = DEFAULT;
DROP TABLE build, probe;

--echo # Outer join.
CREATE TABLE t1 (col1 INTEGER);
CREATE TABLE t2 (col1 INTEGER);
INSERT INTO t1 VALUES (1), (2);
INSERT INTO t2 VALUES (1);
ANALYZE TABLE t1, t2;

--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t1 LEFT JOIN t2 ON t1.col1 = t2.col1;
SELECT * FROM t1 LEFT JOIN t2 ON t1.col1 = t2.col1;
DROP TABLE t1,t2;

CREATE TABLE t1 (col1 INTEGER);
CREATE TABLE t2 (col1 INTEGER);
CREATE TABLE t3 (col1 INTEGER);
INSERT INTO t1 VALUES (1), (2), (3);
INSERT INTO t2 VALUES (1), (2);
INSERT INTO t3 VALUES (1);

ANALYZE TABLE t1, t2, t3;

let $query = SELECT t1.col1, t2.col1, t3.col1 FROM t1
  LEFT JOIN (
    t2 LEFT JOIN t3 ON t2.col1 = t3.col1)
  ON t2.col1 = t1.col1
  ORDER BY t1.col1, t2.col1, t3.col1;

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

DROP TABLE t1, t2, t3;

CREATE TABLE t1 (i INTEGER NOT NULL);
INSERT INTO t1 VALUES (0), (2), (3),(4);
CREATE TABLE t2 (i INTEGER NOT NULL);
INSERT INTO t2 VALUES (0), (1), (3),(4);
CREATE TABLE t3 (i INTEGER NOT NULL);
INSERT INTO t3 VALUES (0), (1), (2), (4);
CREATE TABLE t4 (i INTEGER NOT NULL);
INSERT INTO t4 VALUES (0), (1), (2), (3);
ANALYZE TABLE t1, t2, t3, t4;

let $query = SELECT * FROM
 t1 LEFT JOIN
 ( t2 LEFT JOIN
   ( t3 LEFT JOIN
     t4
     ON t4.i = t3.i
   )
   ON t3.i = t2.i
 )
 ON t2.i = t1.i;

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;

DROP TABLE t1, t2, t3, t4;

CREATE TABLE t1 (col1 INTEGER);
CREATE TABLE t2 (col1 INTEGER);
CREATE TABLE t3 (col1 INTEGER);
ANALYZE TABLE t1, t2, t3;

let $query = SELECT * FROM t1 LEFT JOIN (t2 INNER JOIN t3 ON t2.col1 = t3.col1)
  ON t1.col1 = t2.col1;

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
DROP TABLE t1, t2, t3;

--echo # Set up a case where the join condition in a left join does not refer
--echo # to any of the tables (degenerate join predicate). It will be attached
--echo # as an "extra" condition on the hash join iterator.
CREATE TABLE t1 (col1 INTEGER);
CREATE TABLE t2 (col1 INTEGER);
INSERT INTO t1 VALUES (1);
INSERT INTO t2 VALUES (1);
ANALYZE TABLE t1, t2;

# RAND() always returns a number in the range [0.0, 1.0), so the query is
# deterministic (the condition will always be false).
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t1 LEFT JOIN t2 ON RAND() < -1;
SELECT * FROM t1 LEFT JOIN t2 ON RAND() < -1;
DROP TABLE t1, t2;

CREATE TABLE t1 (col1 INTEGER);
CREATE TABLE t2 (col1 INTEGER, col2 INTEGER);
INSERT INTO t1 VALUES (1);
INSERT INTO t2 VALUES (1, 1), (1, 2), (1, 3), (1, 4), (1, 5);
SELECT * FROM t1 LEFT JOIN t2 ON t1.col1 = t2.col1 AND t2.col2 = 1;
SELECT * FROM t1 LEFT JOIN t2 ON t1.col1 = t2.col1 AND t2.col2 = 2;
SELECT * FROM t1 LEFT JOIN t2 ON t1.col1 = t2.col1 AND t2.col2 = 3;
SELECT * FROM t1 LEFT JOIN t2 ON t1.col1 = t2.col1 AND t2.col2 = 4;
SELECT * FROM t1 LEFT JOIN t2 ON t1.col1 = t2.col1 AND t2.col2 = 5;
SELECT * FROM t1 LEFT JOIN t2 ON t1.col1 = t2.col1 AND t2.col2 = 6;
DROP TABLE t1, t2;

--echo # Run a query where we get a cached outer join condition (i.e. comparison
--echo # between two constant values. The condition is evaluated once and then
--echo # cached.). This condition must be placed as a condition below the
--echo # hash join iterator, and not as a filter after it.
CREATE TABLE t1 (f1 INTEGER, f2 INTEGER);
INSERT INTO t1 VALUES (1,1), (2,2);
ANALYZE TABLE t1;
let $query = SELECT
  tt.t
FROM
  (SELECT 'crash1' AS t, f2 FROM t1) AS tt
  LEFT JOIN t1 ON tt.t = 'crash2' AND tt.f2 = t1.f2
WHERE
  tt.t = 'crash1';
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;
DROP TABLE t1;

--echo # Set up a query where the optimizer will attach an AND condition to ot3,
--echo # where the AND condition is inside a trigger condition guard.
--echo # Additionally, one of the conditions inside the AND condition is a join
--echo # condition while the other is not. What we should see in EXPLAIN is that
--echo # the hash join iterator uses one of these conditions as a join
--echo # condition, while the other condition should be attached as an "extra"
--echo # condition.
CREATE TABLE t1 (col1 INTEGER);
CREATE TABLE t2 (col1 INTEGER);
ANALYZE TABLE t1, t2;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree
  SELECT * FROM t1 LEFT JOIN t2 ON t1.col1 = t2.col1 AND RAND() < RAND();
DROP TABLE t1, t2;

--echo # See that outer join with spill to disk works.
CREATE TABLE t1 (col1 BIGINT NOT NULL, col2 BIGINT NOT NULL);
INSERT INTO t1 SELECT POW(2, 0), POW(2, 0);
let $count=0;
while ($count < 10)
{
  eval INSERT INTO t1
         SELECT col1 + POW(2, $count), col2 + POW(2, $count) FROM t1;
  inc $count;
}
CREATE TABLE t2 AS SELECT * FROM t1;
UPDATE t1 SET col2 = col1 + 1 WHERE MOD(col1, 2) = 0;

ANALYZE TABLE t1, t2;

let $query_no_extra_conditions =
  SELECT SUM(t1.col1), SUM(t1.col2) FROM t1 LEFT JOIN t2 ON t1.col1 = t2.col1;
let $query_with_extra_conditions =
  SELECT SUM(t1.col1), SUM(t1.col2) FROM t1
    LEFT JOIN t2 ON t1.col1 = t2.col1 AND t1.col2 < t2.col2;

SET join_buffer_size = 1024;

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query_no_extra_conditions;
eval $query_no_extra_conditions;

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query_with_extra_conditions;
eval $query_with_extra_conditions;

SET optimizer_switch = DEFAULT;
SET join_buffer_size = DEFAULT;
DROP TABLE t1, t2;

CREATE TABLE t1 (i INTEGER);
CREATE TABLE t2 (i INTEGER);
CREATE TABLE t3 (i INTEGER);
ANALYZE TABLE t1, t2, t3;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree
  SELECT * FROM t1 LEFT JOIN
    (t2 LEFT JOIN t3 ON ABS(t2.i) = t3.i)
  ON t2.i = t1.i;
DROP TABLE t1, t2, t3;

--echo # See that we do not attach conditions that are always 'true' to the hash
--echo # join iterator.
CREATE TABLE t1 (i INTEGER);
CREATE TABLE t2 (i INTEGER);
ANALYZE TABLE t1, t2;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t1 LEFT JOIN t2 ON 1=1;
DROP TABLE t1, t2;

--echo # Set up a query where the join condition is an equi-join condition,
--echo # but it refers to only one side of the hash join (degenerate join
--echo # predicate). This condition must be attached as an 'extra' condition
--echo # (proper join conditions must refer to both sides of the join).
CREATE TABLE t1 (i INTEGER);
CREATE TABLE t2 (i INTEGER);
CREATE TABLE t3 (i INTEGER);
ANALYZE TABLE t1, t2, t3;
--replace_regex $elide_costs
EXPLAIN FORMAT=tree
  SELECT * FROM t1 LEFT JOIN (t2 LEFT JOIN t3 ON TRUE) ON t3.i = t2.i;
DROP TABLE t1, t2, t3;

--echo # Execute a query that causes the hash join to do everything in memory
--echo # with multiple refills of the hash table. This can be triggered when the
--echo # build input does not fit in memory, and the iterator is not allowed to
--echo # spill to disk due to LIMIT in the query.
CREATE TABLE t1 (col1 VARCHAR(255));
INSERT INTO t1 SELECT UUID();
INSERT INTO t1 SELECT UUID() FROM t1;
INSERT INTO t1 SELECT UUID() FROM t1;
INSERT INTO t1 SELECT UUID() FROM t1;
INSERT INTO t1 SELECT UUID() FROM t1;
INSERT INTO t1 SELECT UUID() FROM t1;
INSERT INTO t1 SELECT UUID() FROM t1;
INSERT INTO t1 SELECT UUID() FROM t1;
CREATE TABLE t2 AS SELECT UUID() AS col1 FROM t1;

ANALYZE TABLE t1, t2;

SET join_buffer_size = 128;
let $query = SELECT * FROM t1 LEFT JOIN t2 ON t1.col1 = t2.col1 LIMIT 1000;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;

# The results are not interesting. We only want to see that we do not assert.
--disable_result_log
eval $query;
--enable_result_log

SET join_buffer_size = DEFAULT;
DROP TABLE t1, t2;

--echo #
--echo # Bug #30631730: WL#13377: RESULT DIFF WITH LEFT HASH JOIN
--echo #

CREATE TABLE t1 (pk INTEGER PRIMARY KEY);

INSERT INTO t1 VALUES
  (1),(2),(3),(4),(5),(6),(7),(8),(9),(10),(11),(12),(13),(14),(15),(16),(17),(18),(19),(20);

ANALYZE TABLE t1;

let $query =
  SELECT * FROM t1 AS alias1 LEFT JOIN t1 AS alias2 ON NOT EXISTS (
    SELECT * FROM t1 WHERE alias1.pk NOT IN ( 20 ) );

# Verify that we get a nested loop for the left join (since the inner
# conditions refer to something on the outside, we cannot use hash join),
# but that the antijoin stays a hash join.
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;

DROP TABLE t1;

--echo #
--echo # Bug #30642968: WL#13377: RESULTSET DIFFERENCE WITH HASH ANTIJOIN
--echo #

CREATE TABLE t1 (f1 INTEGER);
INSERT INTO t1 VALUES ( 100 ), ( 200 );
ANALYZE TABLE t1;

let $query =
  SELECT * FROM t1 AS alias1 LEFT JOIN t1 AS alias2 ON alias2.f1 = alias1.f1
  AND NOT EXISTS ( SELECT * FROM t1 AS alias3 WHERE alias1.f1 <= 1 );

# Verify that we get a hash antijoin, a nested loop outer join, and that the
# f1=f1 join condition is properly added (inside the nested loop).
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

DROP TABLE t1;

--echo #
--echo # Bug #30650725: WL13476: RESULTSET DIFFERENCE WITH BKA
--echo #

CREATE TABLE t1 (
  pk int NOT NULL,
  col_datetime datetime
);

INSERT INTO t1 VALUES (9,'1988-02-06 01:32:15');

CREATE TABLE t2 (
  col_int_key int,
  col_datetime datetime,
  KEY idx_N_col_int_key (col_int_key)
);

INSERT INTO t2 VALUES (9,'2025-01-07 03:12:24');

ANALYZE TABLE t1, t2;

SET optimizer_switch='mrr=on,mrr_cost_based=off,batched_key_access=on';

let $query =
  SELECT * FROM
   t1 AS table3 LEFT JOIN (t1 AS table1, t2 AS table2) ON table2.col_int_key=table3.pk;

# Like in the previous bug, verify that the outer join is a nested loop
# (it needs to, because the BKA lookup references it), while the BKA remains
# a BKA.
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

SET optimizer_switch=DEFAULT;

DROP TABLE t1, t2;

--echo #
--echo # Bug #30665042: WL#13476: RESULTSET DIFFERENT NUMBER OF ROWS
--echo #

CREATE TABLE t1 (
  pk INTEGER,
  f1 varchar(1),
  KEY i1 (pk)
);

INSERT INTO t1 VALUES (2,'x');
INSERT INTO t1 VALUES (3,'x');
INSERT INTO t1 VALUES (4,'x');
INSERT INTO t1 VALUES (5,'x');

ANALYZE TABLE t1;

# This test is more complicated than what we are trying to actually test
# (in particular, BKA has nothing to do with it), but it seems to be difficult
# to get the join optimizer to set up the required nested two hash joins
# in a simpler way.
#
# The basic idea we're trying to show is that when there is an extra condition
# that involves inner_t3, the NULL row flag of inner_t1 must also be set to
# false, since the access to inner_t3.pk goes through a view ref (even though
# the derived table is merged) that checks NULLity of the derived table by
# checking inner_t1.
let $query =
  SELECT *
    FROM t1 AS table1
    LEFT JOIN (
      SELECT inner_t3.* FROM
        t1 AS inner_t1
        JOIN t1 AS inner_t2 ON inner_t1.pk = inner_t2.pk
        JOIN t1 AS inner_t3 ON inner_t1.f1 = inner_t2.f1
    ) AS table2 ON NOT EXISTS (
      SELECT * FROM t1 WHERE table2.pk >= 1 OR table1.f1 = 'G'
    );

SET optimizer_switch='mrr=on,mrr_cost_based=off,batched_key_access=on';
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;
SET optimizer_switch=DEFAULT;

DROP TABLE t1;

--echo #
--echo # Bug#30520749 - REGRESSION: LEFT JOIN WITH IMPOSSIBLE ON CONDITION PERFORMS SLOWLY
--echo #

CREATE TABLE t1 ( f1 INTEGER );
INSERT INTO t1 VALUES (1), (2), (3), (4), (5);
CREATE TABLE t2 AS SELECT * FROM t1;
ANALYZE TABLE t1, t2;

# Conditions referring to no sides should be pushed, and the entire right side
# should be removed.
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t1 LEFT JOIN t2 ON 1=2;

# Conditions referring to the right side only should be pushed.
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t1 LEFT JOIN t2 ON t2.f1 > 3;

# Conditions referring to the left side should _not_ be pushed,
# but remain as extra conditions.
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT * FROM t1 LEFT JOIN t2 ON t1.f1 > 3;

DROP TABLE t1, t2;

--echo #
--echo # Bug #30664671: HASH JOINS : CRASH WITH COMPARING DIFFERENT CHARSET
--echo # Bug #30664831: SEVERE PERFORMANCE REGRESSION JOINING ON MEDIUMTEXT COLUMNS!
--echo #

# Set up a (potentially) huge column with a PAD SPACE collation.
CREATE TABLE t1 (
  a LONGTEXT COLLATE utf32_sinhala_ci
);
INSERT INTO t1 (a) VALUES ('abc'), ('abc'), ('abc'), ('abc'), ('abc'), ('abc');
ANALYZE TABLE t1;

--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT 1 FROM t1 JOIN t1 AS t2 ON t1.a=t2.a;
SELECT 1 FROM t1 JOIN t1 AS t2 ON t1.a=t2.a;

--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT 1 FROM t1 LEFT JOIN t1 AS t2 ON t1.a=t2.a;
SELECT 1 FROM t1 LEFT JOIN t1 AS t2 ON t1.a=t2.a;

DROP TABLE t1;

--echo #
--echo # Bug#30716607 SIG11 IN WELL_FORMED_COPY_NCHARS() AT
--echo #              SQL-COMMON/SQL_STRING.CC
--echo # Bug#30648809 HASH_JOIN_BUFFER::LOADINTOTABLEBUFFERS() HITS AN ASSERT
--echo #              AND CRASH
CREATE TABLE t1 (
  str_col VARCHAR(1)
);

CREATE TABLE t2 (
  i INTEGER,
  str_col VARCHAR(1),
  PRIMARY KEY (i),
  KEY idx (str_col)
);

CREATE TABLE t3 (i INTEGER);

INSERT INTO t1 VALUES ('r'), ('s');

INSERT INTO t2 VALUES (9,'s'), (11,'o');

ANALYZE TABLE t1, t2, t3;

# Set up a query where we have a hash join inside an aggregate function.
# The aggregate will set c1_2 to have a NULL row, so hash join will have to
# reset this flag when loading a row back into the table buffer (record[0]).
# If the NULL row flag is not reset, an assertion will be raised. Note that the
# test case is a bit complicated, but this was the most simplified version we
# were able to make.
let $query =
  SELECT 1 FROM t1 RIGHT JOIN t2 ON t2.str_col = t1.str_col
  WHERE (t1.str_col, t1.str_col) NOT IN (
    SELECT c1_1.str_col, MIN(c1_2.str_col)
    FROM t2 AS c1_1 INNER JOIN t2 AS c1_2 STRAIGHT_JOIN t3 ON t3.i = c1_2.i
  );

SET sql_mode = '';
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
# We only want to see that we do not assert, so ignore the results.
--disable_result_log
eval $query;
--enable_result_log
SET sql_mode = DEFAULT;

DROP TABLE t1, t2, t3;

--echo #
--echo # Bug#30738687 CRASH IN HASHJOINCONDITION::HASHJOINCONDITION
--echo #
CREATE TABLE t (
  c TEXT
);

ANALYZE TABLE t;

# The join condition attached to the topmost hash join iterator should not be
# inside a "found_match" trigger.
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT c FROM
(
  SELECT 1 AS c FROM t AS a
  RIGHT JOIN t AS b ON 1
  RIGHT JOIN t AS c ON 1
  RIGHT JOIN t AS d ON 1
) AS z
NATURAL RIGHT JOIN t;

DROP TABLE t;

--echo #
--echo # Bug#30741679 ASSERTION `FALSE' IN EXTRACT_VALUE_FOR_HASH_JOIN
--echo # Bug#30769883 HIT ASSERT IN
--echo #              ITEM_FUNC_EQ::APPEND_JOIN_KEY_FOR_HASH_JOIN()
--echo #
CREATE TABLE t1 (
  a INTEGER,
  b INTEGER
);

CREATE TABLE t2 (
  a INTEGER,
  b INTEGER
);

INSERT INTO t1 VALUES (1, 1), (2, 2), (3, 3), (4, 4), (5, 5), (6, 6);
INSERT INTO t2 VALUES (1, 1);
ANALYZE TABLE t1;

# See that conditions having a non-scalar subquery is attached as extra
# conditions, and not as join conditions.
let $query = SELECT * FROM t1 as table1
  WHERE (SELECT a, b FROM t2 WHERE table1.a <> 6) IN (SELECT a, b FROM t1);
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# Conditions having a scalar subquery (subquery with one column) should be used
# as a proper join condition.
let $query = SELECT * FROM t1 as table1
  WHERE (SELECT a FROM t2 WHERE table1.a <> 6) IN (SELECT a FROM t1);
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

DROP TABLE t1;
DROP TABLE t2;

--echo #
--echo # Bug#30792727 ERRORS WHEN BUILDING HASH TABLE FOR HASH JOIN NOT
--echo #              PROPAGATED
--echo #
CREATE TABLE t (i INTEGER);
INSERT INTO t VALUES (1), (2);
ANALYZE TABLE t;

# The first argument to JSON_EXTRACT should be a JSON document. An error
# is expected when building the hash table for t1 since
# "JSON_EXTRACT(t1.i, t1.i)" is used as the hash table key (giving a number as
# the first argument results in an error).
let $query = SELECT * FROM t t1, t t2 WHERE JSON_EXTRACT(t1.i, t1.i) = t2.i;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;

--error ER_INVALID_TYPE_FOR_JSON
eval $query;

# See that only one error was raised.
# Hypergraph joins the tables in opposite order, so gets different
# number of errors.
SHOW ERRORS;
DROP TABLE t;

--echo #
--echo # Bug #31102789 A PREDICATE THAT COMPARES 0 WITH -0
--echo #               YIELDS AN INCORRECT RESULT
--echo #

CREATE TABLE t0(c0 INT);
CREATE TABLE t1(c0 DOUBLE);
CREATE TABLE t2(c0 DOUBLE);
INSERT INTO t0 VALUES(0);
INSERT INTO t1 VALUES('-0');
INSERT INTO t2 VALUES('+0');

SELECT * FROM t0, t1 WHERE t0.c0 = t1.c0;
SELECT * FROM t1, t2 WHERE t1.c0 = t2.c0;
SELECT * FROM t0, t2 WHERE t0.c0 = t2.c0;

DROP TABLE t0, t1, t2;

--echo #
--echo # Bug #31376809: PERFORMANCE REGRESSION FROM 5.7->8.0, DUE TO ANTIJOIN OF NOT EXISTS SUBQUERY
--echo #

CREATE TABLE t1 (a INTEGER NOT NULL, b INTEGER);
INSERT INTO t1 VALUES (1,1),(2,2),(3,3),(4,4),(5,5);
ANALYZE TABLE t1;

# Verify that:
#  a) There's a LIMIT 1 being put on the hash.
#  b) The outer side of the hash join is never executed.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE SELECT t1.a FROM t1 LEFT JOIN t1 AS t2 ON TRUE WHERE t2.a IS NULL;

DROP TABLE t1;

--echo #
--echo # Bug #31563414: REGRESSION: ITEM_FUNC_EQ::APPEND_JOIN_KEY_FOR_HASH_JOIN CONST: ASSERTION `FALSE' FAILED.
--echo #

CREATE TABLE t1 (a INTEGER NOT NULL);
INSERT INTO t1 VALUES (1),(2),(3),(4);
ANALYZE TABLE t1;

let $query =
  SELECT *
    FROM t1 AS a
    LEFT JOIN t1 AS b ON FALSE
    LEFT JOIN t1 AS c ON b.a=c.a;

# Check that we get two prunings; first, prune away b due to the ON FALSE
# join condition, and then, prune away c due to the equijoin condition
# requiring b.
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;

eval $query;

DROP TABLE t1;

--echo #
--echo # Bug #31940549: NEW MEMORY LEAK ON TRUNK RELATING TO HASHJOINITERATOR::BUILDHASHTABLE , OPEN_CACHED_FILE
--echo #

SET join_buffer_size=32768;
CREATE TABLE t(a DOUBLE NOT NULL, b VARCHAR(15873) NOT NULL);
INSERT INTO t VALUES(1, REPEAT('abs',1000));
INSERT INTO t VALUES(2, REPEAT('ajs',1000));
INSERT INTO t VALUES(3, REPEAT('ajj',1000));
INSERT INTO t VALUES(4, REPEAT('jjs',1000));
INSERT INTO t SELECT * FROM t;
INSERT INTO t SELECT * FROM t;

--disable_warnings
# Enables probe row saving (due to the LIMIT) on a subquery
# that is run multiple times per query.
SELECT
(
  SELECT 1 FROM (SELECT t_outer.a) Z LEFT JOIN t
  ON t.a > t.b LIMIT 1
) FROM t AS t_outer;
--enable_warnings

DROP TABLE t;
SET join_buffer_size=DEFAULT;

--echo #
--echo # Bug #31832001 HASH JOIN MISMATCHES SIGNED AND UNSIGNED
--echo #

CREATE TABLE t2 (c1 BIGINT NOT NULL);
CREATE TABLE t13 (c1 BIGINT UNSIGNED NOT NULL);
INSERT INTO t2 VALUES (-1),(1);
INSERT INTO t13 VALUES (18446744073709551615),(1);

SELECT * FROM t2 JOIN t13 WHERE t2.c1=t13.c1;
SELECT /*+ NO_BNL(t2,t13) */ * FROM t2 JOIN t13 WHERE t2.c1=t13.c1;

DROP TABLE t2, t13;

--echo #
--echo # Bug #32244974: SIG 6 IN HASHJOINITERATOR::READROWFROMPROBEITERATOR() AT HASH_JOIN_ITERATOR.CC
--echo #

CREATE TABLE t1 (
  pk INTEGER,
  col_int INTEGER,
  PRIMARY KEY (pk)
);

CREATE TABLE t2 (
  pk INTEGER,
  col_int INTEGER
);

INSERT INTO t1 VALUES (2,6);
INSERT INTO t1 VALUES (3,NULL);

INSERT INTO t2 VALUES (1,6);

ANALYZE TABLE t1, t2;

--error ER_SUBQUERY_NO_1_ROW
SELECT 1
FROM
  t1 AS alias1
  JOIN t2 AS alias2 ON alias1.col_int = alias2.col_int
  JOIN t1 AS alias3 ON 1
  WHERE
    ( SELECT 1 UNION SELECT MAX(alias1.pk) ) = alias3.pk;

DROP TABLE t1, t2;

--echo # Bug#32579184: Hash join: my_lengthsp_utf32: assertion (length % 4)

CREATE TABLE t (
  a TEXT CHARACTER SET ucs2 COLLATE ucs2_sinhala_ci,
  b TEXT CHARACTER SET utf32 COLLATE utf32_german2_ci
);
INSERT INTO t(a, b) VALUES('h','k'), ('w','e'), ('a','b'), ('c','d'), ('e','f'), ('x','y');
ANALYZE TABLE t;
let $query =
SELECT 1 FROM t AS a INNER JOIN t AS b ON a.a=b.b;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=TREE $query;
eval $query;
DROP TABLE t;

--echo # Bug#33204161: Strings with ASCII charset and UCS2 don't match in join

CREATE TABLE t1(wstr_name VARCHAR(64) COLLATE ucs2_general_ci);
CREATE TABLE t2(str_name VARCHAR(64) COLLATE ascii_general_ci);

INSERT INTO t1 VALUES('9337a53f-cfd6-4a68-a1fc-e2d84eba8079');
INSERT INTO t2 VALUES('9337a53f-cfd6-4a68-a1fc-e2d84eba8079');

ANALYZE TABLE t1, t2;

let $query =
SELECT wstr_name, str_name
FROM t1 INNER JOIN t2 ON t1.wstr_name = t2.str_name;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=TREE $query;
eval $query;

DROP TABLE t1, t2;

--echo # Bug#33787914: int-decimal. using left join, the returned result is incorrect

CREATE TABLE t(a DECIMAL(5, 3));
INSERT INTO t VALUES (0.1), (0.2), (0.3), (0.01), (0.91), (0.001), (0.991);
--sorted_result
SELECT * FROM t AS t1, t AS t2 WHERE 1 - t1.a = 1 - t2.a;
--sorted_result
SELECT * FROM t AS t1, t AS t2 WHERE t1.a = t2.a;
DROP TABLE t;

--echo # Bug#33794977: Wrong results from hash join with compound hash keys

CREATE TABLE t(a VARCHAR(10), b VARCHAR(10));
INSERT INTO t VALUES ('x', 'xx'), ('xx', 'x');
--sorted_result
SELECT * FROM t AS t1, t AS t2 WHERE t1.a = t2.a AND t1.b = t2.b;
DROP TABLE t;

CREATE TABLE t(a DECIMAL(3,0), b DECIMAL(3,0));
INSERT INTO t
VALUES (0, 128), (128, 0), (382, 1), (1, 382), (-1, 385), (385, -1);
--sorted_result
SELECT * FROM t AS t1, t AS t2 WHERE t1.a = t2.a AND t1.b = t2.b;
DROP TABLE t;

--echo # Bug#33638843: Hypergraph Offload issue : Invalid Join condition

CREATE TABLE t1 (f1 INTEGER);
CREATE TABLE t2 (f1 INTEGER);
CREATE VIEW vmerge AS SELECT 1 AS f1, f1 AS b_f1 FROM t2;

INSERT INTO t1 VALUES (1),(2),(3),(4),(5);
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t2 SELECT * FROM t1;
ANALYZE TABLE t1, t2;

ANALYZE TABLE t1, t2;

--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=tree SELECT * FROM t1 LEFT JOIN vmerge AS v ON t1.f1 = v.f1;

DROP TABLE t1,t2;
DROP VIEW vmerge;

--echo # Bug#34352156: Hypergraph: Assert PassesConflictRules() failed in
--echo #               join_optimizer.cc

CREATE TABLE t1 (f1 INTEGER, f2 INTEGER);
# This induces a conflict rule {t3} -> {t1,t2} for the edge between
# t4->t5 because of the left join. When exploring the join between
# {t4} and {t2,t3,t5} it has a valid edge in (t2.f2 = t4.f2) where as
# the cycle inducing edge (t4.f2 = t5.f1) does not pass the conflict
# rule.
# This statement would earlier assert expecting the conflict rule
# for the other edge to always pass.
SELECT 1
 FROM t1 JOIN ((t1 AS t2 LEFT JOIN t1 AS t3 ON t2.f2 = t3.f2)
 JOIN (t1 AS t4 JOIN t1 AS t5 ON t4.f2 = t5.f1) ON t2.f2 = t5.f1)
 ON t1.f2 = t5.f2;
DROP TABLE t1;

--echo #
--echo # Bug#34837464: Incorrect Left Outer Join Execution
--echo #

CREATE TABLE t1(varchar_col VARCHAR(10), decimal_col DECIMAL(10, 0));
INSERT INTO t1 VALUES ('', 0), ('', 0), ('', 0), ('', 0), ('', 0), ('', 0);

CREATE TABLE t2(id INT, varchar_col VARCHAR(10), decimal_col DECIMAL(10, 0));
INSERT INTO t2 VALUES (1, NULL, NULL), (2, NULL, NULL), (3, NULL, NULL),
(4, NULL, NULL), (5, NULL, NULL), (6, NULL, NULL);

ANALYZE TABLE t1, t2;

# These two hash joins used to compare decimal zero or the empty
# string in t1 as equal to NULL in t2, and return wrong results.

--let $query = SELECT id FROM t1 LEFT JOIN t2 USING (varchar_col)
--replace_regex $elide_costs
--eval EXPLAIN FORMAT=TREE $query
--eval $query

--let $query = SELECT id FROM t1 LEFT JOIN t2 USING (decimal_col)
--replace_regex $elide_costs
--eval EXPLAIN FORMAT=TREE $query
--eval $query

DROP TABLE t1, t2;

--echo #
--echo # Bug#33824300: hypergraph: Wrong results from hash join with
--echo #               long string types
--echo #

CREATE TABLE t(b LONGBLOB);
INSERT INTO t VALUES
  (0x0800), (0x04b9), (0x0c00), (0x06cb), (0x1000), (0x0f6b), (0x0b73);
ANALYZE TABLE t;
let $query =
  SELECT HEX(t1.b), HEX(t2.b) FROM t AS t1, t AS t2 WHERE t1.b = t2.b;
--replace_regex $elide_costs
--eval EXPLAIN FORMAT=TREE $query
--sorted_result
--eval $query
DROP TABLE t;

--echo #
--echo # Bug#34940000 Hash join execution may be ineficcient if probe input is empty
--echo #

CREATE TABLE t1 (
  a INT NOT NULL,
  b INT NOT NULL
);

INSERT INTO t1 VALUES (1,1),(1,2),(2,1);
INSERT INTO t1 SELECT * FROM t1;
INSERT INTO t1 SELECT * FROM t1;

ANALYZE TABLE t1;

# We should check if the probe (outer) input is empty first if it is a left
# join or antijoin. If the probe is empty, we can skip reading the build input.

--echo # Plan change with new hypergraph cost model.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE FORMAT=tree SELECT * FROM t1 x1 LEFT JOIN t1 x2 ON x1.a=x2.a
  WHERE x1.b < 0;

--echo # Plan change with new hypergraph cost model.
--replace_regex $elide_costs_and_time
EXPLAIN ANALYZE FORMAT=tree SELECT * FROM t1 x1 WHERE x1.a < 0 AND x1.a NOT IN
  (SELECT a FROM t1 x2 WHERE x2.b = x1.b);

DROP TABLE t1;

--echo #
--echo # Bug#35703114 Assertion `m_psi_batch_mode == PSI_BATCH_MODE_NONE'
--echo # failed.
--echo #

CREATE TABLE t1(i1 int primary key, i2 int, c varchar(10));

INSERT INTO t1 VALUES (1,1,'a'), (2,2,'a'), (3,3,'a'), (4,4,'a'), (5,5,'a');
INSERT INTO t1 SELECT 5 + t.i1, 5 + t.i2, t.c FROM t1 AS t;
INSERT INTO t1 SELECT 10 + t.i1, 10 + t.i2, t.c FROM t1 AS t;
INSERT INTO t1 SELECT 20 + t.i1, 20 + t.i2, t.c FROM t1 AS t;

ANALYZE TABLE t1;

# To reproduce the bug we need a query with these properties:
# - It should be planned as a hash join.
# - We should start reading from the probe input (i.e. outer input).
#   Therefore we use a left join.
# - There should be a filter on the probe input.
# - Evaluating this filter should give an error for the first row.
#   Therefore we use an invalid Unicode code point.
let $query =
    SELECT * FROM t1 x1 LEFT JOIN t1 x2 ON x1.i2=x2.i2
    WHERE x1.c <= CHAR(0xd83f);

--replace_regex $elide_costs
--eval EXPLAIN FORMAT=TREE $query

--error ER_CANNOT_CONVERT_STRING
--eval $query

DROP TABLE t1;

--echo #
--echo # Bug#35789589 Assertion `m_index_cursor.is_positioned()' failed.
--echo #

SET optimizer_switch = 'materialization=off,derived_merge=off';

CREATE TABLE bb (
  pk INT auto_increment,
  col_int INT,
  col_int_key INT,
  col_varchar VARCHAR(1),
  col_varchar_key VARCHAR(1),
  PRIMARY KEY(pk)
);

CREATE TABLE cc (
  pk INT AUTO_INCREMENT,
  col_int_key INT,
  col_varchar_key VARCHAR(1),
  PRIMARY KEY(pk)
);

INSERT INTO cc (pk,col_int_key,col_varchar_key) VALUES
  (DEFAULT,867110528,'8'),
  (DEFAULT,-1926290568,'9'),
  (DEFAULT,428174454,'f'),
  (DEFAULT,127754401,'a'),
  (DEFAULT,-923329804,'p');

INSERT INTO bb (pk,col_int,col_int_key,col_varchar,col_varchar_key) VALUES
  (DEFAULT,-1777898150,-1744771058,'b','Z');

ANALYZE TABLE bb, cc;

# To reproduce this issue we need a query where the query plan has:
# * A left hash-join H1. The first row it produces must have a
#   NULL-row right half.
# * Another hash join H2 with H1 as its probe input. H2 must start with
#   reading from the probe input (i.e. H1).
# * A third hash-join H3. H3 must have H2 as its build input.
#
# With such a plan, H3 calls H2.Init() which causes the first
# (partial null) row to be read from H1. H3 then calls
# H2.SetNullRow(false), which erroneously resets the null flag in the
# H1 row. Producing such a plan was tricky, so the somewhat complex query
# below is taken from the bug report.

SELECT alias1.col_int_key AS field2 FROM
  (bb AS alias1 RIGHT OUTER JOIN
        (((SELECT SQL_SMALL_RESULT SQ2_alias1.* FROM
          (bb AS SQ2_alias1 INNER JOIN bb AS SQ2_alias2 ON
            (SQ2_alias2.col_int_key = SQ2_alias1.col_int_key))
          WHERE SQ2_alias1.col_int < SQ2_alias2.col_int) AS alias2
          RIGHT OUTER JOIN bb AS alias3 ON
            (alias3.col_varchar_key = alias2.col_varchar_key))) ON
          (alias3.col_varchar_key = alias2.col_varchar_key))
        WHERE EXISTS ((SELECT SQ3_alias1.col_int_key AS SQ3_field1 FROM
          (cc AS SQ3_alias1 STRAIGHT_JOIN
            (cc AS SQ3_alias2 INNER JOIN bb AS SQ3_alias3 ON
              (SQ3_alias3.col_varchar = SQ3_alias2.col_varchar_key))
            ON (SQ3_alias3.pk = SQ3_alias2.pk))));

DROP TABLE bb, cc;
SET optimizer_switch=DEFAULT;

--echo #
--echo # Bug#31969607: THE RESULT IS INCONSISTENT BETWEEN JOIN AND GROUP BY
--echo #

CREATE TABLE t1 (id INT PRIMARY KEY AUTO_INCREMENT, j JSON);
INSERT INTO t1(j) VALUES
  (NULL), ('-1'), ('1'), ('1.0'), ('"1"'), ('[1, 2]'), ('[1.0, 2.0]'),
  ('9223372036854775806'), ('9223372036854775807'), ('"abc"'), ('"Abc"');
CREATE TABLE t2 (id INT PRIMARY KEY AUTO_INCREMENT, vc VARCHAR(100));
INSERT INTO t2(vc) VALUES
  (NULL), ('-1'), ('1'), ('1.0'), ('"1"'), ('[1, 2]'), ('[1.0, 2.0]'),
  ('9223372036854775806'), ('9223372036854775807'), ('abc'), ('Abc');
CREATE TABLE t3 (i BIGINT);
INSERT INTO t3 VALUES (NULL), (-1), (0), (1), (2), (3),
                      (9223372036854775806), (9223372036854775807);

--sorted_result
SELECT * FROM t1 AS a, t1 AS b WHERE a.j = b.j;
--sorted_result
SELECT * FROM t1 AS a, t1 AS b WHERE a.j <=> b.j;

--sorted_result
SELECT * FROM t1, t2 WHERE t1.j = t2.vc;
--sorted_result
SELECT * FROM t1, t2 WHERE t1.j <=> t2.vc;

--sorted_result
SELECT * FROM t1, t3 WHERE t1.j = t3.i;
--sorted_result
SELECT * FROM t1, t3 WHERE t1.j <=> t3.i;

DROP TABLE t1, t2, t3;
