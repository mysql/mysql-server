--echo # Test of the JSON functions used in SQL statements
--echo # ----------------------------------------------------------------------

SET NAMES utf8;
CREATE TABLE t1 (pk int  NOT NULL PRIMARY KEY AUTO_INCREMENT, i INT, j JSON);
INSERT INTO t1(i, j) VALUES (0, NULL);
INSERT INTO t1(i, j) VALUES (1, '{"a": 2}');
INSERT INTO t1(i, j) VALUES (2, '[1,2]');
INSERT INTO t1(i, j) VALUES (3, '{"a":"b", "c":"d","ab":"abc", "bc": ["x", "y"]}');
INSERT INTO t1(i, j) VALUES (4, '["here", ["I", "am"], "!!!"]');
INSERT INTO t1(i, j) VALUES (5, '"scalar string"');
INSERT INTO t1(i, j) VALUES (6, 'true');
INSERT INTO t1(i, j) VALUES (7, 'false');
INSERT INTO t1(i, j) VALUES (8, 'null');
INSERT INTO t1(i, j) VALUES (9, '-1');
INSERT INTO t1(i, j) VALUES (10, CAST(CAST(1 AS UNSIGNED) AS JSON));
INSERT INTO t1(i, j) VALUES (11, '32767');
INSERT INTO t1(i, j) VALUES (12, '32768');
INSERT INTO t1(i, j) VALUES (13, '-32768');
INSERT INTO t1(i, j) VALUES (14, '-32769');
INSERT INTO t1(i, j) VALUES (15, '2147483647');
INSERT INTO t1(i, j) VALUES (16, '2147483648');
INSERT INTO t1(i, j) VALUES (17, '-2147483648');
INSERT INTO t1(i, j) VALUES (18, '-2147483649');
INSERT INTO t1(i, j) VALUES (19, '18446744073709551615');
INSERT INTO t1(i, j) VALUES (20, '18446744073709551616');
INSERT INTO t1(i, j) VALUES (21, '3.14');
INSERT INTO t1(i, j) VALUES (22, '{}');
INSERT INTO t1(i, j) VALUES (23, '[]');
INSERT INTO t1(i, j) VALUES (24, CAST(CAST('2015-01-15 23:24:25' AS DATETIME) AS JSON));
INSERT INTO t1(i, j) VALUES (25, CAST(CAST('23:24:25' AS TIME) AS JSON));
INSERT INTO t1(i, j) VALUES (26, CAST(CAST('2015-01-15' AS DATE) AS JSON));
INSERT INTO t1(i, j) VALUES (27, CAST(TIMESTAMP'2015-01-15 23:24:25' AS JSON));
INSERT INTO t1(i, j) VALUES (28, CAST(ST_GeomFromText('POINT(1 1)') AS JSON));
# auto-convert to utf8mb4
INSERT INTO t1(i, j) VALUES (29, CAST('[]' AS CHAR CHARACTER SET 'ascii'));
INSERT INTO t1(i, j) VALUES (30, CAST(x'cafe' AS JSON));
INSERT INTO t1(i, j) VALUES (31, CAST(x'cafebabe' AS JSON));


--echo #
--echo # Test of JSON comparator.
--echo #

SELECT i,
       (j = '"scalar string"') AS c1,
       (j = 'scalar string') AS c2,
       (j = CAST('"scalar string"' AS JSON)) AS c3,
       (j = CAST(CAST(j AS CHAR CHARACTER SET 'utf8mb4') AS JSON)) AS c4,
       (j = CAST(NULL AS JSON)) AS c5,
       (j = NULL) AS c6,
       (j <=> NULL) AS c7,
       (j <=> CAST(NULL AS JSON)) AS c8,
       (j IN (-1, 2, 32768, 3.14)) AS c9,
       (j IN (CAST('[1, 2]' AS JSON), CAST('{}' AS JSON), CAST(3.14 AS JSON))) AS c10,
       (j = (SELECT j FROM t1 WHERE j = CAST('null' AS JSON))) AS c11,
       (j = (SELECT j FROM t1 WHERE j IS NULL)) AS c12,
       (j = (SELECT j FROM t1 WHERE 1<>1)) AS c13,
       (j = DATE'2015-01-15') AS c14,
       (j = TIME'23:24:25') AS c15,
       (j = TIMESTAMP'2015-01-15 23:24:25') AS c16,
       (j = CURRENT_TIMESTAMP) AS c17,
       (j = ST_GeomFromText('POINT(1 1)')) AS c18,
       (JSON_EXTRACT(j, '$.a') = 2) AS c19
FROM t1
ORDER BY i;

SELECT i FROM t1
  WHERE j = CAST(CAST(j AS CHAR CHARACTER SET 'utf8') AS JSON)
  ORDER BY i;
SELECT CAST(NULL AS UNSIGNED) = CAST(NULL AS JSON);
SELECT CAST(NULL AS JSON) = CAST(NULL AS JSON);
SELECT CAST(NULL AS JSON) = NULL;
SELECT CAST(1 AS JSON) = NULL;
SELECT CAST('true' AS JSON) = 1;
SELECT CAST('true' AS JSON) = true;

SELECT a.i, b.i, a.j < b.j, a.j = b.j, a.j > b.j, a.j <> b.j, a.j <=> b.j
FROM t1 a, t1 b
ORDER BY a.i, b.i;

--echo # Verify that the index on the int column is not used when
--echo # comparing the int column to a JSON column. The two columns
--echo # should be compared using the JSON comparator.
CREATE TABLE t2(i int, j json);
CREATE INDEX t2_i ON t2(i);
INSERT INTO t2 values (1, CAST(1 AS JSON));
INSERT INTO t2 values (1, CAST('"1"' AS JSON));
ANALYZE TABLE t2;
let $query=SELECT * FROM t2 where i = j;
eval EXPLAIN $query;
eval $query;
DROP TABLE t2;

# Create a table full of JSON numeric scalars to verify that the JSON
# comparator returns the expected result when comparing all
# combinations of those values.
#
# The values should be inserted in ascending order. The table has a
# rank column that tells how the comparator is expected to order the
# JSON values. If two rows have the same rank, the comparator is
# expected to say that the JSON values on the two rows are equal. If a
# row has a lower rank than another, the JSON value in that row is
# expected to be smaller than the JSON value in the other row.
CREATE TABLE numbers(id INT NOT NULL AUTO_INCREMENT,
                     `rank` INT,
                     j JSON,
                     PRIMARY KEY(id));
INSERT INTO numbers(`rank`, j) VALUES
(1, '-1e100'),
(2, '-1e65'),
# smallest DECIMAL (negative with 65 digits)
(3, CAST(-99999999999999999999999999999999999999999999999999999999999999999 AS JSON)),
(4, CAST(-9223372036854776001 AS JSON)),
(5, CAST(-9223372036854776000 AS JSON)),
# closest DOUBLE approximation of the smallest SIGNED BIGINT
(5 /* same rank as previous */, '-9.223372036854776e18'),
(6, CAST(-9223372036854775999 AS JSON)),
(7, CAST(-9223372036854775809 AS JSON)),   # smallest SIGNED BIGINT - 1
(8, CAST(-9223372036854775808 AS JSON)),   # smallest SIGNED BIGINT
(9, CAST(-9223372036854775807 AS JSON)),   # smallest SIGNED BIGINT + 1
(10, '-1e-50'),                  # close to zero, fits in a DECIMAL
(11, '-1.2345678901234e-71'),    # has to be truncated to fit in a DECIMAL
(12, CAST(-0.000000000000000000000000000000000000000000000000000000000000000000000012 AS JSON)),
(12 /* same rank as previous */, '-1.2e-71'),
(13, '-1.0345678901234e-71'),    # has to be truncated to fit in a DECIMAL
(14, '-1e-100'),                 # too close to zero to fit in a DECIMAL
(15, '0'),
(15 /* same rank as previous */, '0.0'),
(15 /* same rank as previous */, '-0.0'),
(15 /* same rank as previous */, CAST(0.0 AS JSON)),
(15 /* same rank as previous */, CAST(CAST(-0.0e0 AS DECIMAL) AS JSON)),
(16, '1e-100'),                  # too close to zero to fit in a DECIMAL
(17, '1.0345678901234e-71'),     # has to be truncated to fit in a DECIMAL
(18, CAST(0.000000000000000000000000000000000000000000000000000000000000000000000012 AS JSON)),
(18 /* same rank as previous */, '1.2e-71'),
(19, '1.2345678901234e-71'),     # has to be truncated to fit in a DECIMAL
(20, '1e-50'),                   # close to zero, fits in a DECIMAL
(21, CAST(9223372036854775806 AS JSON)),    # largest SIGNED BIGINT - 1
(22, CAST(9223372036854775807 AS JSON)),    # largest SIGNED BIGINT
(23, CAST(9223372036854775808 AS JSON)),    # largest SIGNED BIGINT + 1
(24, CAST(9223372036854775999 AS JSON)),
# closest DOUBLE approximation of the largest SIGNED BIGINT
(25, '9.223372036854776e18'),
(25 /* same rank as previous */, CAST(9223372036854776000 AS JSON)),
(26, CAST(9223372036854776001 AS JSON)),
(27, CAST(18446744073709551614 AS JSON)),   # largest UNSIGNED BIGINT - 1
(28, CAST(18446744073709551615 AS JSON)),   # largest UNSIGNED BIGINT
(29, CAST(18446744073709551616 AS JSON)),   # largest UNSIGNED BIGINT + 1
# Gets converted to the closest DOUBLE approximation of UNSIGNED BIGINT + 1
# by the JSON parser
(30, '18446744073709551616'),
# biggest DECIMAL (65 digits)
(31, CAST(99999999999999999999999999999999999999999999999999999999999999999 AS JSON)),
(32, CAST('1e65' AS JSON)),
(33, CAST('1e100' AS JSON));
SELECT *, JSON_TYPE(j) FROM numbers ORDER BY id;

# Now compare every combination of scalars in the table using <, =, >,
# <> and <=>, and cross-check the results against the ranks. The query
# returns the rows where the comparison returned an unexpected result.
# If all is well, the query returns no rows.
SELECT a.j, b.j, a.j < b.j, a.j = b.j, a.j > b.j, a.j <=> b.j
FROM numbers a, numbers b
WHERE ((a.j < b.j) <> (a.`rank` < b.`rank`)) OR
      ((a.j = b.j) <> (a.`rank` = b.`rank`)) OR
      ((a.j > b.j) <> (a.`rank` > b.`rank`)) OR
      ((a.j <=> b.j) <> (a.`rank` <=> b.`rank`));

DROP TABLE numbers;

# Verify handling of errors during evaluation of the arguments to the
# comparator, both in the left argument and in the right argument.
CREATE TABLE t(txt TEXT);
INSERT INTO t VALUES ('');
--error ER_INVALID_JSON_TEXT_IN_PARAM
SELECT COUNT(*) FROM t WHERE JSON_EXTRACT(txt, '$') = 5;
--error ER_INVALID_JSON_TEXT_IN_PARAM
SELECT COUNT(*) FROM t WHERE 5 = JSON_EXTRACT(txt, '$');
DROP TABLE t;

--echo #
--echo # WL#8539 - Ordering of scalar JSON values
--echo #

# Create some timestamps.
CREATE TABLE timestamps (ts TIMESTAMP(6));
INSERT INTO timestamps VALUES
('2000-01-01 00:00:00'),
('2000-01-01 00:00:00.01'),
('2000-01-01 00:00:00.001'),
('2000-01-01 00:00:00.002'),
('2000-01-01 00:00:00.02'),
('2000-01-01 23:59:59.999999'),
('2000-01-02 00:00:00'),
('2000-02-01 00:00:00'),
('2010-12-02 01:00:00'),
('2010-12-02 01:02:00'),
('2010-12-02 01:02:03'),
('2010-12-02 02:01:00'),
('1970-01-02 00:00:01'),
('1970-01-02 00:00:01.000001');
SELECT * FROM timestamps ORDER BY CAST(ts AS JSON);

# Create datetimes that correspond to the above timestamps, and add some values
# that are outside the accepted range of the timestamp data type.
CREATE TABLE datetimes (dt DATETIME(6));
INSERT INTO datetimes SELECT ts FROM timestamps;
INSERT INTO datetimes VALUES
('1960-01-02 03:04:05'),
('1960-01-02 03:04:06'),
('1000-01-01 00:00:00'),
('9999-12-31 23:59:59.999999');
SELECT * FROM datetimes ORDER BY CAST(dt AS JSON);

# Create some times using the time component of the above datetimes. Also add
# some times that go outside of the 0-24 range of the time component of
# datetime.
CREATE TABLE times (t TIME(6));
INSERT INTO times SELECT DISTINCT TIME(dt) FROM datetimes;
INSERT INTO times VALUES
('-838:59:59'),
('838:59:59'),
('-00:00:00.000001'),
('-00:00:00'),
('24:00:00'),
('-12:00:00'),
('-24:00:00');
SELECT * FROM times ORDER BY CAST(t AS JSON);

# Create dates using the date component of the above datetimes.
CREATE TABLE dates(d DATE);
INSERT INTO dates SELECT DISTINCT DATE(dt) FROM datetimes;

# Create some signed integers.
CREATE TABLE signed_integers(i BIGINT);
INSERT INTO signed_integers VALUES
(0), (1), (2), (3), (4), (5), (10), (11), (12), (20), (21), (22),
(99), (100), (101), (999), (1000), (1001),
(9223372036854775806), (9223372036854775807);
INSERT INTO signed_integers SELECT -i FROM signed_integers;
INSERT INTO signed_integers VALUES (-9223372036854775808);
SELECT * FROM signed_integers ORDER BY CAST(i AS JSON);

# Create some unsigned integers.
CREATE TABLE unsigned_integers(i BIGINT UNSIGNED);
INSERT INTO unsigned_integers SELECT i FROM signed_integers where i >= 0;
INSERT INTO unsigned_integers VALUES
(9223372036854775808), (18446744073709551614), (18446744073709551615);
SELECT * FROM unsigned_integers ORDER BY CAST(i AS JSON);

# Create some decimals.
CREATE TABLE decimals (d DECIMAL(25,3));
INSERT INTO decimals SELECT i FROM signed_integers;
INSERT INTO decimals SELECT i FROM unsigned_integers;
INSERT INTO decimals VALUES
(9223372036854776000), (-9223372036854776000),
(9223372036854776001), (-9223372036854776001),
(3.13), (3.14), (3.15), (-3.13), (-3.14), (-3.15),
(3.131), (3.141), (3.151), (-3.131), (-3.141), (-3.151),
(3.129), (3.139), (3.149), (-3.129), (-3.139), (-3.149),
(0.1), (0.01), (0.001), (-0.1), (-0.01), (-0.001);
SELECT * FROM decimals ORDER BY CAST(d AS JSON);

# Create some doubles.
CREATE TABLE doubles (d DOUBLE);
INSERT INTO doubles SELECT d FROM decimals;
INSERT INTO doubles VALUES
(1.5E-200), (1.5E200), (-1.5E-200), (-1.5E200),
(-1E-323), (-1E-322), (-1E-321), (1E-323), (1E-322), (1E-321),
(-1E308), (-1E307), (-1E306), (1E308), (1E307), (1E306);
SELECT * FROM doubles ORDER BY CAST(d AS JSON);

# Now convert all of the above values to JSON.
CREATE TABLE t(id INT PRIMARY KEY AUTO_INCREMENT, j JSON);
INSERT INTO t(j) SELECT CAST(ts AS JSON) FROM timestamps ORDER BY ts;
INSERT INTO t(j) SELECT CAST(dt AS JSON) FROM datetimes ORDER BY dt;
INSERT INTO t(j) SELECT CAST(t AS JSON) FROM times ORDER BY t;
INSERT INTO t(j) SELECT CAST(d AS JSON) FROM dates ORDER BY d;
INSERT INTO t(j) SELECT CAST(i AS JSON) FROM signed_integers ORDER BY i;
INSERT INTO t(j) SELECT CAST(i AS JSON) FROM unsigned_integers ORDER BY i;
INSERT INTO t(j) SELECT CAST(d AS JSON) FROM decimals ORDER BY d;
INSERT INTO t(j) SELECT CAST(d AS JSON) FROM doubles ORDER BY d;

# Insert some more JSON values.
INSERT INTO t(j) VALUES
(NULL), (NULL), ('true'), ('false'), ('null'),
('"abc"'), ('""'), ('"abcd"'), ('"bc"'),
('"abc\\u0000\\u0000"'), ('"abc\\u0000"'),
('0.0'), ('-0.0'), ('9223372036854776000'),
('1.0e-1'), ('1.0e-2'),
(CAST(0.000000000000001 AS JSON)),
(CAST(0.00000000000000115 AS JSON)),
(CAST(0.0000000000000001 AS JSON)),
(CAST(0.000000000000000116 AS JSON)),
(CAST(0.0 AS JSON)),
(CAST(-999999999999999999999999999999999999999999999999999999999999999999999999999999999 AS JSON)),
(CAST(-999999999999999999999999999999999999999999999999999999999999999999999999999999998 AS JSON)),
(CAST(-999999999999999999999999999999999999999999999999999999999999999999999999999999997 AS JSON)),
(CAST(999999999999999999999999999999999999999999999999999999999999999999999999999999997 AS JSON)),
(CAST(999999999999999999999999999999999999999999999999999999999999999999999999999999998 AS JSON)),
(CAST(999999999999999999999999999999999999999999999999999999999999999999999999999999999 AS JSON)),
(CAST(-1E81 AS JSON)),
(CAST(-9.99E80 AS JSON)),
(CAST(9.99E80 AS JSON)),
(CAST(1E81 AS JSON)),
(JSON_ARRAY('an array')),
(JSON_ARRAY('another array')),
(JSON_OBJECT('an', 'object')),
(JSON_OBJECT('another', 'object')),
(CAST(ST_GeomFromText('POINT(0 0)') AS JSON)),
(CAST(ST_GeomFromText('POINT(0 1)') AS JSON)),
(CAST(CAST('1234abcd' AS BINARY) AS JSON));

--disable_query_log
# Minimal buffer size, to test merging as well
set @@sort_buffer_size=32 * 1024;
--enable_query_log

# Now order the table on the JSON column.
SELECT j, JSON_TYPE(j) AS tp FROM t ORDER BY j, id;
SELECT j, JSON_TYPE(j) AS tp FROM t ORDER BY j, id limit 2 offset 2;

SELECT j, JSON_TYPE(j) AS tp FROM t ORDER BY j DESC, id;
SELECT j, JSON_TYPE(j) AS tp FROM t ORDER BY j DESC, id limit 2 offset 2;

# Ordering on a JSON expression should give the same result.
SELECT JSON_EXTRACT(j, '$') AS je, JSON_TYPE(j) AS tp FROM t ORDER BY je, id;

# Set max_sort_length as small as possible and order again. Since we'll now just
# look at a prefix of the values, distinct values with a common prefix may order
# as equal.
SET @@max_sort_length=8;
SELECT j, JSON_TYPE(j) AS tp FROM t ORDER BY j, id;
SELECT j, JSON_TYPE(j) AS tp FROM t ORDER BY j, id limit 2 offset 2;
SET @@max_sort_length=default;

--disable_query_log
set @@sort_buffer_size=default;
--enable_query_log

# GROUP BY uses a temporary for grouping, GROUP BY WITH ROLLUP uses filesort to
# do the grouping.
ANALYZE TABLE t;
EXPLAIN SELECT j, COUNT(*) FROM t GROUP BY j ORDER BY j;
EXPLAIN SELECT j, COUNT(*) FROM t GROUP BY j WITH ROLLUP;

# Grouping produces indeterminate results based on the order of evaluation. For example,
# either '2' or '2.0' could be the name of the group
# either '20' or '20.000' could be the name of the group
# either '-0.1' or '-0.100' could be the name of the group
# either '9223372036854775807.000' or '9223372036854775807' could be the name of the group
# These issues are resolved by removing trailing zeros from decimals and replacing -0 with 0.
# This also replaces .000010 with 010 and .000001 with 001 [because \s is not supported].
--replace_regex /1\.0e\-1/0.1/ /1\.0e\-2/0.01/ /0\.010/0.01/ /\.000// /\.0// /0\.100/0.1/ /\-0/0/ /3\.130/3.13/ /3\.140/3.14/ /3\.150/3.15/ /9\.223372036854776e18/9223372036854776000/
SELECT j, COUNT(*) FROM t GROUP BY j ORDER BY j;

--replace_regex /1\.0e\-1/0.1/ /1\.0e\-2/0.01/ /0\.010/0.01/ /\.000// /\.0// /0\.100/0.1/ /\-0/0/ /3\.130/3.13/ /3\.140/3.14/ /3\.150/3.15/ /9\.223372036854776e18/9223372036854776000/
SELECT JSON_EXTRACT(j, '$') AS je, COUNT(*) FROM t GROUP BY je ORDER BY je;

--replace_regex /1\.0e\-1/0.1/ /1\.0e\-2/0.01/ /0\.010/0.01/ /\.000// /\.0// /0\.100/0.1/ /\-0/0/ /3\.130/3.13/ /3\.140/3.14/ /3\.150/3.15/ /9\.223372036854776e18/9223372036854776000/
SELECT j, COUNT(*) FROM t GROUP BY j WITH ROLLUP;

--replace_regex /1\.0e\-1/0.1/ /1\.0e\-2/0.01/ /0\.010/0.01/ /\.000// /\.0// /0\.100/0.1/ /\-0/0/ /3\.130/3.13/ /3\.140/3.14/ /3\.150/3.15/ /9\.223372036854776e18/9223372036854776000/
SELECT JSON_EXTRACT(j, '$') AS je, COUNT(*) FROM t GROUP BY je WITH ROLLUP;

DROP TABLE t, timestamps, datetimes, times, dates, signed_integers,
           unsigned_integers, decimals, doubles;

# Test ordering of a not nullable column.
CREATE TABLE t(j JSON NOT NULL);
INSERT INTO t VALUES ('1'), ('2'), ('10'), ('"1"'), ('"2"'), ('"10"'),
                     ('true'), ('false'), ('null');
SELECT j FROM t ORDER BY j;
SELECT j FROM t ORDER BY JSON_EXTRACT(j, '$');
SELECT JSON_EXTRACT(j, '$') FROM t ORDER BY 1;

# Ordering on (j+1) will convert to a numeric type.
SELECT j FROM t ORDER BY j+1, JSON_TYPE(j);
DROP TABLE t;

CREATE TABLE t(vc varchar(10));
INSERT INTO t VALUES ('["abc"]'), ('[1');
--error ER_INVALID_JSON_TEXT_IN_PARAM
SELECT * FROM t ORDER BY CAST(vc AS JSON);
--error ER_INVALID_JSON_TEXT_IN_PARAM
SELECT * FROM t ORDER BY JSON_EXTRACT(vc, '$[0]');
--error ER_INVALID_JSON_TEXT_IN_PARAM
SELECT CAST(vc AS JSON) AS j FROM t ORDER BY j;
--error ER_INVALID_JSON_TEXT_IN_PARAM
SELECT JSON_EXTRACT(vc, '$[0]') AS j FROM t ORDER BY j;
--error ER_INVALID_JSON_TEXT_IN_PARAM
SELECT CAST(vc AS JSON) FROM t ORDER BY 1;
--error ER_INVALID_JSON_TEXT_IN_PARAM
SELECT JSON_EXTRACT(vc, '$[0]') FROM t ORDER BY 1;
DROP TABLE t;

--echo #
--echo # Internal ordering of arrays and objects. Ordered by cardinality.
--echo #
CREATE TABLE t(i int, j json);
INSERT INTO t VALUES
(1, '{}'), (2, '{"a":1}'), (3, '{"ab":2}'), (4, '{"a":1,"b":2}'),
(5, '{"c":3,"d":4}'), (6, '{"a":1,"b":2,"c":3,"d":4}');
INSERT INTO t VALUES
(1, '[]'), (2, '[1]'), (3, '[2]'), (4, '[1,2]'), (5, '[2,1]'), (6, '[1,2,3]'),
(7, '[1,2,3,4]'), (8, '[4,3,2,1]'), (9, '[1,2,3,4,5]');
INSERT INTO t SELECT i+100, j FROM t;
SELECT * FROM t ORDER BY j, i;
SELECT * FROM t ORDER BY j DESC, i;
# GROUP BY knows how to distinguish the arrays and the objects, even
# if they have the same cardinality.
# Group by produces indeterminate results based on the order the items are evaluated
# SELECT j, COUNT(*) FROM t GROUP BY j ORDER BY j;
# GROUP BY WITH ROLLUP, on the other hand, doesn't know how to
# distinguish them, and produces confusing results for arrays/objects.
# GROUP BY WITH ROLLUP is only useful on scalar results for now.
SELECT j, COUNT(*) FROM t GROUP BY j WITH ROLLUP;
DROP TABLE t;

--echo # Test NULLs sorting.
CREATE TABLE t(i int, j json);
INSERT INTO t(i) VALUES (1),(2),(3),(2),(1);
SELECT * FROM t ORDER BY j, i;
SELECT * FROM t ORDER BY j DESC, i;
SELECT i, JSON_EXTRACT(j, '$') AS je FROM t ORDER BY je, i;
SELECT i, JSON_EXTRACT(j, '$') AS je FROM t ORDER BY je DESC, i;
INSERT INTO t(i, j) VALUES (1, '1');
SELECT * FROM t ORDER BY j, i;
SELECT * FROM t ORDER BY j DESC, i;
SELECT i, JSON_EXTRACT(j, '$') AS je FROM t ORDER BY je, i;
SELECT i, JSON_EXTRACT(j, '$') AS je FROM t ORDER BY je DESC, i;
DROP TABLE t;

# Merging of sort results should not get confused if one of the sort columns is
# a JSON column.
CREATE TABLE t(vc TEXT, j JSON);
INSERT INTO t (vc) VALUES ('a'), ('b'), ('c');
INSERT INTO t SELECT * FROM t;
INSERT INTO t SELECT * FROM t;
INSERT INTO t SELECT * FROM t;
INSERT INTO t SELECT * FROM t;
INSERT INTO t SELECT * FROM t;
INSERT INTO t SELECT * FROM t;
INSERT INTO t SELECT * FROM t;
SELECT * FROM t ORDER BY vc, j;
DROP TABLE t;

--echo # ----------------------------------------------------------------------
--echo # Test of JSON_VALID function.
--echo # ----------------------------------------------------------------------

--echo
--echo # Table - Json string column - utf-8, NULL
--echo Note: 'utf8' is a subset of internal 'utf8mb4'
--echo
create table utf8_t (c varchar(20)) CHARACTER SET 'utf8';
insert into utf8_t values (NULL);
-- echo # Expect NULL:
select JSON_VALID(c) from utf8_t;
delete from utf8_t;

--echo
--echo # Table - Json string column - utf-8, valid
insert into utf8_t values ('[123]');
select JSON_VALID(c) from utf8_t;
delete from utf8_t;

--echo
--echo # Table - Json string column - utf-8, non-utf8
insert into utf8_t values ('[123');
--echo expect 0 (false)
select JSON_VALID(c) from utf8_t;
delete from utf8_t;

--echo
--echo # Table - Try to extract JSON from TIMESTAMP column
ALTER TABLE utf8_t ADD d TIMESTAMP;

--echo # Should give false; not string or JSON type
--echo # and we do not convert automatically from TIMESTAMP to JSON
insert into utf8_t values (NULL, '2014-11-25 18:00');
select JSON_VALID(d) from utf8_t;

--echo # Explicit cast to a character data type
--echo # allows MySQL to parse this is a JSON text
--echo # The string isn't a legal JSON document, tho, so not valid.
select JSON_VALID(CAST(d as CHAR)) from utf8_t;

--echo # Should give true
select JSON_VALID(CONCAT( CONCAT('"', CAST(d as CHAR)), '"')) from utf8_t;
delete from utf8_t;
drop table utf8_t;

--echo
--echo # Table - JSON type; should give true by definition
create table json_t(t json);
insert into json_t values ('[123]');
select json_VALID(t) from json_t;


--echo
--echo # Function result - JSON
select JSON_VALID( JSON_ARRAY(t, t) ) from json_t;

drop table json_t;

--echo # ----------------------------------------------------------------------
--echo # Test of JSON_LENGTH function.
--echo # ----------------------------------------------------------------------

create table utf8_mj_length (a int, c varchar(20)) CHARACTER SET 'utf8';
insert into utf8_mj_length values( 1, null );
insert into utf8_mj_length values( 2, '1' );
insert into utf8_mj_length values( 3, 'abc' );
insert into utf8_mj_length values( 4, '"abc"' );
insert into utf8_mj_length values ( 5, 'true' );
insert into utf8_mj_length values ( 6, 'false' );
insert into utf8_mj_length values ( 7, 'null' );

select a, c, json_length( c ) from utf8_mj_length where a = 1;

select a, c, json_length( c ) from utf8_mj_length where a = 2;

--echo
--echo # invalid json text
--error ER_INVALID_JSON_TEXT_IN_PARAM
select a, c, json_length( c ) from utf8_mj_length where a = 3;

select a, c, json_length( c ) from utf8_mj_length where a = 4;
select a, c, json_length( c ) from utf8_mj_length where a = 5;
select a, c, json_length( c ) from utf8_mj_length where a = 6;
select a, c, json_length( c ) from utf8_mj_length where a = 7;

create table json_mj_length( a int, b json );

insert into json_mj_length values( 1, NULL );

select a, b, json_length( b ) from json_mj_length where a = 1;

# json_length() with vacuous path expressions

set names 'ascii';

--echo
--echo # path auto-converted to a utf8 string from ascii
--echo
select a, c, json_length( c, '$' ) from utf8_mj_length where a = 2;

set names 'utf8';

select a, c, json_length( c, '$' ) from utf8_mj_length where a = 1;
select a, c, json_length( c, '$' ) from utf8_mj_length where a = 2;

--echo
--echo # invalid json text
--error ER_INVALID_JSON_TEXT_IN_PARAM
select a, c, json_length( c, '$' ) from utf8_mj_length where a = 3;

select a, c, json_length( c, '$' ) from utf8_mj_length where a = 4;
select a, c, json_length( c, '$' ) from utf8_mj_length where a = 5;
select a, c, json_length( c, '$' ) from utf8_mj_length where a = 6;
select a, c, json_length( c, '$' ) from utf8_mj_length where a = 7;

select a, b, json_length( b, '$' ) from json_mj_length where a = 1;

drop table utf8_mj_length;
drop table json_mj_length;

# different paths for each row
CREATE TABLE json_remove_t(j JSON, p TEXT);
INSERT INTO json_remove_t(p) VALUES ('$.a'), ('$.b'), ('$.c');
UPDATE json_remove_t SET j = '{"a":1,"b":2,"c":3}';
SELECT j, p, json_remove(j, p) FROM json_remove_t ORDER BY p;
DROP TABLE json_remove_t;

CREATE TABLE json_merge_t(i INT, j JSON);
INSERT INTO json_merge_t VALUES
(0, NULL),
(1, 'true'),
(2, '5'),
(3, '[1,2]'),
(4, '{"a":["x", "y"]}'),
(5, '{"a":"b","c":"d"}');
SELECT t1.j, t2.j,
       JSON_MERGE_PRESERVE(t1.j, t2.j) AS m1,
       JSON_MERGE_PRESERVE(t2.j, t1.j) AS m2
FROM json_merge_t t1, json_merge_t t2 ORDER BY t1.i, t2.i;
DROP TABLE json_merge_t;

create table keys1(i int, j json);
insert into keys1 select i, j from t1;

DROP TABLE t1;

# example from the wl7909 spec

create table rawOrders( orderID int, doc json );
insert into rawOrders values ( 1, '100' ), ( 2, '{ "id": 2, "quantity": 200 }' );

create table orders( orderID int, quantity int unsigned );

INSERT INTO orders( orderID, quantity )
  SELECT
   r.orderID,
   CASE( JSON_TYPE( r.doc ) )
     WHEN "INTEGER" THEN CAST( r.doc AS UNSIGNED INT )
     WHEN "OBJECT" THEN CAST( JSON_EXTRACT( r.doc, '$.quantity' ) AS UNSIGNED INT )
     ELSE NULL
   END
  FROM rawOrders r;

select * from rawOrders order by orderID;
select * from orders order by orderID;

drop table rawOrders;
drop table orders;

# the value here isn't important, but it should be stable
select charset(json_type('{}'));


--echo # ----------------------------------------------------------------------
--echo # Test of CAST(<column> AS JSON)
--echo # ----------------------------------------------------------------------
create table t1(dati datetime, da date,
                tim time, ts timestamp,
                y year,
                --
                ti tinyint,   tiu tinyint unsigned,
                si smallint,  siu smallint unsigned,
                mi mediumint, miu mediumint unsigned,
                i  int,       iu  int unsigned,
                bi bigint,    biu bigint unsigned,
                boo boolean,
                --
                dc decimal(5,2),
                n numeric(5,2),
                --
                f float, d double,
                bitt bit(10),
                blb blob,
                bin binary(10),
                en enum('a','b','c'),
                se set('a','b','c'),
                --
                ge geometry,
                po point,
                ls linestring,
                py polygon,
                js json
                );


insert into t1 values('2014-11-25 18:00', '2014-11-25',
                      '18:00:59', '2014-11-25 18:00',
                      '1999',
                      --
                      127, 255,
                      32767, 65535,
                      8388607, 16777215, -- 3 bytes
                      2147483647, 4294967295, -- 4 bytes
                      9223372036854775807, 18446744073709551615,
                      true,
                                            --
                      3.14,
                      3.14,
                      --
                      3.14, 3.14,
                      b'10101',
                      '10101abcde',
                      '10101abcde',
                      'b',
                      'a,c',
                      --
                      ST_GeomFromText('POINT(1 1)'),
                      ST_GeomFromText('POINT(1 1)'),
                      ST_GeomFromText('LINESTRING(0 0,1 1,2 2)'),
                      ST_GeomFromText('POLYGON((0 0,10 0,10 10,0 10,0 0),
                                            (5 5,7 5,7 7,5 7, 5 5))'),
                      '[123]'
                      );

select json_type(cast(dati as json)) from t1;
select json_type(cast(da as json)) from t1;
select json_type(cast(tim as json)) from t1;
select json_type(cast(ts as json)) from t1;

select json_type(cast(y as json)) from t1;
select json_type(cast(ti as json)) from t1;
select json_type(cast(tiu as json)) from t1;
select json_type(cast(si as json)) from t1;
select json_type(cast(siu as json)) from t1;
select json_type(cast(mi as json)) from t1;
select json_type(cast(miu as json)) from t1;
select json_type(cast(i as json)) from t1;
select json_type(cast(iu as json)) from t1;
select json_type(cast(bi as json)) from t1;
select json_type(cast(biu as json)) from t1;
select json_type(cast(boo as json)) from t1; # INTEGER (not enough info)

select json_type(cast(dc as json)) from t1;
# select json_type(cast(n as json)) from t1;

select json_type(cast(f as json)) from t1;
select json_type(cast(d as json)) from t1;

select json_type(cast(bitt as json)) from t1;
select json_type(cast(blb as json)) from t1;
select json_type(cast(bin as json)) from t1;

select json_type(cast(en as json)) from t1;
select json_type(cast(se as json)) from t1;

select json_type(cast(ge as json)) from t1;
select json_type(cast(po as json)) from t1;
select json_type(cast(ls as json)) from t1;
select json_type(cast(py as json)) from t1;

select json_type(cast(js as json)) from t1;

#
# same, but now show the printable value:
#
select cast(dati as json) from t1;
select cast(da as json) from t1;
select cast(tim as json) from t1;
select cast(ts as json) from t1;

select cast(y as json) from t1;
select cast(ti as json) from t1;
select cast(tiu as json) from t1;
select cast(si as json) from t1;
select cast(siu as json) from t1;
select cast(mi as json) from t1;
select cast(miu as json) from t1;
select cast(i as json) from t1;
select cast(iu as json) from t1;
select cast(bi as json) from t1;
select cast(biu as json) from t1;
select cast(boo as json) from t1; # INTEGER (not enough info)

select cast(dc as json) from t1;
# select cast(n as json) from t1;

select cast(f as json) from t1;
select cast(d as json) from t1;

select cast(bitt as json) from t1;
select cast(blb as json) from t1;
select cast(bin as json) from t1;

select cast(en as json) from t1;
select cast(se as json) from t1;

select cast(ge as json) from t1;
select cast(po as json) from t1;
select cast(ls as json) from t1;
select cast(py as json) from t1;

select cast(js as json) from t1;

--echo #
--echo # Bug#21442878 INCORRECT RETURN STATUS FROM
--echo #              ITEM_JSON_TYPECAST::VAL_JSON() ON PARSE ERRORS
--echo #
--error ER_INVALID_TYPE_FOR_JSON
select json_extract(en, '$') from t1;

drop table t1;

create table t1 ( c1 varchar(200) character set 'latin1',
                  c2 varchar(200) character set 'utf8' );
insert into t1 values ('[1,2]',  # legal json, but not utf-8
                       '[1,2 '); # illegal json, but utf-8

# convert latin1 to UTF-8
select cast(c1 as json) from t1;
--error ER_INVALID_JSON_TEXT_IN_PARAM
select cast(c2 as json) from t1;
--error ER_INVALID_JSON_TEXT_IN_PARAM
select cast(c2 as json) is null from t1;

drop table t1;

# Two distinct but related bugs detected by Knut 2015-02-05 caused NULL for y here:
create table t2(x int);
insert into t2 values (1), (2);
select x, cast(y as json) from (select x, cast(x as json) as y from t2) s order by x;
select x, cast(y as json) from (select x, cast(cast(x as json) as char charset utf8) as y from t2) s order by x;

drop table t2;

--echo # ----------------------------------------------------------------------
--echo # Test of CAST(<select> AS JSON)
--echo # ----------------------------------------------------------------------
# positive test cases
select cast((select 1) as json);

create table t(i int, j json, c char(10) character set 'utf8');
insert into t values (5, '6', '{}');
select cast((select i from t) as json);
select cast((select j from t) as json);
select cast((select c from t) as json);
select cast((select cast(i as json) from t) as json);
select cast((select cast(j as json) from t) as json);
select cast((select cast(c as json) from t) as json);
insert into t values (7, '8', '[]');
--error ER_SUBQUERY_NO_1_ROW
select cast((select i from t) as json);

# Test what happens if the subquery returns NULL. The casts should
# return SQL NULL.
delete from t;
insert into t values (null, null, null);
select cast((select i from t) as json);
select cast((select j from t) as json);
select cast((select cast(i as json) from t) as json);
select cast((select cast(j as json) from t) as json);
select cast((select cast(c as json) from t) as json);

# negative test cases
--error ER_OPERAND_COLUMNS
select cast((select i,i from t) as json);
--error ER_OPERAND_COLUMNS
select cast((select * from t) as json);
drop table t;

--echo # ----------------------------------------------------------------------
--echo # Test of JSON_KEYS function.
--echo # ----------------------------------------------------------------------

select i, json_keys(j) from keys1 order by i;

delete from keys1;
insert into keys1 values (0, NULL),
                         (1, '{"a": 1, "b": {"e": "foo", "b": 3}}');
select i, json_keys(j), json_keys(j, '$.b') from keys1 order by i;
select cast(j as char) from keys1 order by i;

create table t(i int);
select cast(json_extract(j, '$.b.b') as char) from keys1 order by i;
insert into t select cast(json_extract(j, '$.b.b') as char) from keys1;
select * from t order by i;
drop table t;
drop table keys1;

# positive test cases
create table t(j json);
insert into t values ('[ 1, 2, 3, {"a": [4,5,6]}]');
select json_array_append(j, '$[3].a', cast(7 as json)) from t;
select json_array_append(j, '$', 7) from t;
select json_array_append(j, '$', cast(7 as json), '$[3].a', 3.14) from t;
--echo # second path's append ignored since it doesn't specify an array
--echo # nor is it an existing scalar,  so no auto-wrapping either
select json_array_append(j, '$', 7, '$[3].b', cast(8 as json)) from t;
drop table t;

# path caching and leg popping
create table jdoc( id int, doc json );
insert into jdoc values
( 1, '[ [ true ], [ false ] ]' ),
( 2, '[ [ 0 ], [ 1 ] ]' ),
( 3, '[ [ "abc" ], [ "def" ] ]' );

select id, json_array_insert( doc, '$[0][1]', 'fred' )
from jdoc order by id;

select id, json_array_insert( doc, '$[1][0]', 'fred' )
from jdoc order by id;

drop table jdoc;

create table t( id int, v varchar(10));
insert into t values (1, 'a'), (2, null), (3, 'a');
select id v, json_array_insert('[[1]]', '$[0][0]', v) from t order by id;
drop table t;

--echo #
--echo # Bug #21304639: JSON_SET() WITH MULTI-LEG PATH RETURNS DIFFERENT
--echo #                RESULTS ON FIRST ROW VS NEXT
--echo #
create table t21304639(pk int);
insert into t21304639 values (2), (1), (3);
select json_set(
         json_object('existing', pk),
         '$.key_b.test',
         json_object('new', 'apple')
       ) as field1 from t21304639 order by field1;

select json_set(
         json_object('existing', pk),
         '$.key_b.test',
         json_object('new', 'apple')
       ) as field1 from t21304639 order by field1;

drop table t21304639;

create table t (i int, j json, d double);
insert into t values (3, '["a", "b"]', 3.14);
select json_array(i, j, d) from t;
drop table t;

# Array with the smallest possible signed integer and the largest possible
# unsigned integer.
CREATE TABLE t(j JSON);
INSERT INTO t VALUES (JSON_ARRAY(-9223372036854775808, 18446744073709551614));
SELECT * FROM t;
DROP TABLE t;

# examples from the wl7909 spec
create table department( id int, deptName varchar(50), isExempt boolean, blobColumn blob );
insert into department values ( 405, 'Accounting', true, '<a><b>ccc</b><d></d></a>' );

# returns ["Accounting", {"processed": true }]
SELECT JSON_ARRAY( d.deptName, CAST( '{ "processed" : true }' AS JSON ) )
FROM department d
WHERE id = 405;

# stores a JSON value in a JSON-typed column
create table jsn_table( json_column json );
INSERT INTO jsn_table( json_column )
  SELECT JSON_ARRAY( d.deptName, d.id, d.blobColumn )
  FROM department d
  WHERE id = 405;
drop table jsn_table;

drop table department;

create table misc_dt
(
  id int, py polygon
);

insert into misc_dt values
(
  1, ST_GeomFromText('POLYGON((0 0,10 0,10 10,0 10,0 0),
    (5 5,7 5,7 7,5 7, 5 5))')
),
(
  2, null
);

select id, json_array( true, py, false ) from misc_dt order by id;

drop table misc_dt;

# construct from data in a table
create table jro
(
  a int,
  b varchar( 10 ),
  c boolean
);
insert into jro( a, b, c ) values
( 0, 'zero', false ),
( 1, 'one', true ),
( null, null, null );

select a, json_object( 'a', a, 'b', b, 'c', c )
from jro
order by a;

drop table jro;

create table jro2( a int, b varchar( 10 ), c json );
insert into jro2 ( a, b, c ) values
( 1, 'array', '[ 1, 2, 3 ]' ), ( 2, 'object', '{ "d": "foo", "e": true }' );

select a, json_object( 'type', b, 'value', c )
from jro2 order by a;

drop table jro2;

# examples from the wl7909 spec
create table department( id int, deptName varchar(50), isExempt boolean, blobColumn blob );
insert into department values ( 405, 'Accounting', true, '<a><b>ccc</b><d></d></a>' );

# returns {"deptName": "Accounting", "id": 405, "isExempt": true, "date": 2014-11-0400:00:00.000000}
SELECT JSON_OBJECT
(
  'deptName', d.deptName,
  'id', d.id,
  'isExempt', d.isExempt and true
)
FROM department d
WHERE id = 405;

drop table department;

# key names which aren't strings

create table misc_dt
(
  py polygon
);

insert into misc_dt values
(
  ST_GeomFromText('POLYGON((0 0,10 0,10 10,0 10,0 0),
    (5 5,7 5,7 7,5 7, 5 5))')
);

--error ER_INVALID_JSON_CHARSET
select json_object( py, 'def' ) from misc_dt;

drop table misc_dt;


create table json_search_table( id_col int, json_col json );
insert into json_search_table values
( 1, '{ "a": "foobar" }' ),
( 2, '{ "a": "foobar", "b": "focus", "c": [ "arm", "foot", "shoulder" ] }' );

select id_col, json_search( json_col, 'all', 'foo%' )
from json_search_table
order by id_col;

select id_col, json_search( json_col, 'all', 'foot' )
from json_search_table
order by id_col;

select id_col, json_search( json_col, 'all', 'f__us' )
from json_search_table
order by id_col;

# tests with path arguments
delete from json_search_table;
insert into json_search_table values
( 1, '{ "a": "foobar" }' ),
( 2, '{ "a": [ "foolish", "folly", "foolhardy"  ], "b" : "fool" }' );

select id_col, json_search( json_col, 'all', 'foo%', null, '$.a' )
from json_search_table
order by id_col;
select id_col, json_search( json_col, 'all', 'foo%', null, '$.a', '$.b' )
from json_search_table
order by id_col;
select id_col, json_search( json_col, 'one', 'foo%', null, '$.a', '$.b' )
from json_search_table
order by id_col;

delete from json_search_table;
insert into json_search_table values
( 1, '{ "a": "foobar" }' ),
( 2, '[ { "a": { "b": { "c": "fool" } } }, { "b": { "c": "shoulder" } }, { "c": { "c": "food"} } ]' );

select id_col, json_search( json_col, 'all', 'foo%', null, '$.a', '$**.c' )
from json_search_table
order by id_col;
select id_col, json_search( json_col, 'one', 'foo%', null, '$.a', '$**.c' )
from json_search_table
order by id_col;

drop table json_search_table;

# verify that the double-quoted strings returned by json_search()
# are valid path expressions when unpacked via json_unquote().

create table jep( key_col int primary key, doc json, path varchar( 50 ) );
insert into jep values
( 1, '{ "onepotato": "seven"  }', '$.onepotato' ),
( 2, '{ "one potato": "seven"  }', '$."one potato"' ),
( 3, '{ "one \\"potato": "seven"  }', '$."one \\"potato"' ),
( 4, '{ "one \\npotato": "seven"  }', '$."one \\npotato"' );

select key_col,
       json_search( doc, 'all', 'seven' ) paths,
       json_unquote( cast( json_search( doc, 'all', 'seven' ) as char ) ) unquoted,
       path
from jep order by key_col;

drop table jep;

--echo # ----------------------------------------------------------------------
--echo # Test of CASE and IF expressions returning JSON
--echo # ----------------------------------------------------------------------
create table t(j json);
insert into t values (null), ('[3,4,5]');

select json_type(case (j is null) when 1 then
                cast('null' as json) else
                cast('[1,2,3]' as json) end) from t order by j;

# no else clause
select json_type(case (j is null) when 1 then cast(1 as json) end) from t order by j;

select json_type( if(j is null,
                    cast('{"a": 6}' as json),
                    cast('[1,2,3]' as json))) from t order by j;

select json_type( if(j is null,
                    NULL,
                    cast('[1,2,3]' as json))    ) from t order by j;
--echo # ----------------------------------------------------------------------
--echo # Test of CASE and IF expressions with mix of JSON and other types
--echo # Common result type is VARCHAR
--echo # ----------------------------------------------------------------------

select json_type(case (j is null) when 1 then
                3.14 else
                cast('[1,2,3]' as json) end) from t order by j;

select case (j is null) when 1 then
       3.14 else
       cast('[1,2,3]' as json) end from t order by j;

select case (j is null) when 1 then
       'foobar' else
       cast('[1,2,3]' as json) end from t order by j;

select json_type( if(j is null,
                 3.14,
                 cast('[1,2,3]' as json))) from t order by j;

select if(j is null,
          3.14,
          cast('[1,2,3]' as json)) from t order by j;


--echo # ----------------------------------------------------------------------
--echo # Test of IFNULL
--echo # ----------------------------------------------------------------------
select json_type(ifnull(j, cast(3 as json))) from t order by j;
select ifnull(j, cast(3 as json)) from t order by j;      # json_type masked a bug
select json_type(ifnull(NULL, cast(3 as json)));
select json_type(ifnull(cast(3 as json), NULL));
--error ER_INVALID_JSON_TEXT_IN_PARAM
SELECT JSON_TYPE(IFNULL(JSON_EXTRACT(CONCAT(t1.j, 'abc'), '$'), t2.j))
FROM t t1, t t2;
--error ER_INVALID_JSON_TEXT_IN_PARAM
SELECT JSON_TYPE(IFNULL(t1.j, JSON_EXTRACT(CONCAT(t2.j, 'abc'), '$')))
FROM t t1, t t2;

--echo # ----------------------------------------------------------------------
--echo # Json values used in text contexts
--echo # ----------------------------------------------------------------------
delete from t;
insert into t values (NULL), (cast('"aBc"' as json));
select upper(j) from t order by j;
delete from t;
insert into t values (cast(1 as json)), (cast(10 as json)), (cast(2 as json));
select * from t order by j;

select max(j) from t;
select json_type(max(j)) from t;
select min(j) from t;
select json_type(max(j)) from t;

# if we want another sorting, cast to suitable type
select max(cast(j as unsigned)) from t;
--error ER_INVALID_TYPE_FOR_JSON
select json_type(max(cast(j as unsigned))) from t;
drop table t;

--echo # ----------------------------------------------------------------------
--echo # Test JSON arguments and return values of stored functions
--echo # ----------------------------------------------------------------------

create function make_message
(
 sender varchar(50),
 receiver varchar(50),
 subject text,
 received datetime,
 body text
)
returns json
language sql deterministic no sql
return json_object
(
  'sender', sender,
  'receiver', receiver,
  'subject', subject,
  'received', received,
  'body', body
);

create function extract_date( message json )
returns datetime
language sql deterministic no sql
return json_extract( message, '$.received' );

create table messages
(
 id int,
 raw_message json
);

insert into messages(id, raw_message) values
(
 1,
 make_message
  (
   'fred',
   'alice',
   'lunch today?',
   timestamp( '2015-05-11 09:30:05' ),
   'How about lunch at 11:30?'
  )
),
(
 2,
 make_message
  (
   'alice',
   'fred',
   're: lunch today?',
   timestamp( '2015-05-11 09:45:05' ),
   'Sorry. I am in meetings all day long.'
  )
),
(
 3,
 json_object
  (
    'sender', 'fred',
    'receiver', 'alice',
    'subject', 're: lunch today?',
    'received', timestamp( '2015-05-11 09:50:05' ),
    'body', 'Oh, bummer.'
  )
)
;
select * from messages order by id;

# should be DATETIME
select json_type
(
  json_extract
  (
    json_object
    (
      'sender', 'fred',
      'receiver', 'alice',
      'subject', 'lunch today?',
      'received', timestamp( '2015-05-11 09:45:05' ),
      'body', 'How about lunch at 11:30?'
    ),
    '$.received'
  )
) received_type
;

select id, extract_date( raw_message ) extracted_date
from messages order by id;

create function show_received_type( message json )
returns tinytext
language sql deterministic no sql
return json_type( json_extract( message, '$.received' ) );

# should be DATETIME
select show_received_type
(
 json_object
  (
    'sender', 'fred',
    'receiver', 'alice',
    'subject', 're: lunch today?',
    'received', timestamp( '2015-05-11 09:50:05' ),
    'body', 'Oh, bummer.'
  )
) received_type;

# should be DATETIME
select show_received_type
(
     make_message
     (
      'fred',
      'alice',
      'lunch today?',
      timestamp( '2015-05-11 09:30:05' ),
      'How about lunch at 11:30?'
     )
) received_type;

# should be DATETIME
select id, show_received_type( raw_message ) received_type
from messages order by id;

drop function show_received_type;
drop function make_message;
drop function extract_date;
drop table messages;

--echo # Test a function that fails.
CREATE FUNCTION func_that_fails() RETURNS JSON
LANGUAGE SQL DETERMINISTIC NO SQL
RETURN '[not valid json]';
--error ER_INVALID_JSON_TEXT
SELECT JSON_EXTRACT(func_that_fails(), '$');
DROP FUNCTION func_that_fails;

# test a more complicated stored function which declares a JSON variable

delimiter //;
create function get_types( input_value json )
returns json
language sql deterministic contains sql
begin
  declare array_length integer;
  declare return_value json;
  declare idx int;
  declare path varchar(100);

  set array_length = json_length( input_value );
  set return_value = json_array();
  set idx = 0;

  while idx < array_length do
    set path = concat( '$[', idx, ']' );
    set return_value = json_array_append
    (
      return_value,
      '$',
      json_type( json_extract( input_value, path ) )
    );

    set idx = idx + 1;
  end while;

  return return_value;
end//


delimiter ;//

create table blob_table( blob_col blob );
insert into blob_table values( '10101abcde' );

select json_type( dt.a ), dt.a
from
( select get_types
  (
    json_array
    (
      cast( '{}' as json ),
      cast( '[]' as json ),
      'null',
      true,
      1,
      2.3,
      timestamp( '2015-05-11 09:30:05' ),
      cast('23:24:25' as time),
      cast('2015-01-15' as date),
      b'10101',
      blob_col
    )
  ) a
  from blob_table
) dt;

drop table blob_table;
drop function get_types;

delimiter //;
create procedure merge_docs
(
  inout inout_value json
)
begin
  set inout_value = json_object();
end//
delimiter ;//


delimiter //;
create procedure merge_doc_types()
begin
  declare proc_inout json;
  declare tmp_types varchar(100);

  set proc_inout = null;

  call merge_docs( proc_inout );
  set tmp_types = json_type( proc_inout );
end//
delimiter ;//

call merge_doc_types();

drop procedure merge_doc_types;
drop procedure merge_docs;

delimiter //;
create function get_types( input_value json )
returns json
language sql deterministic contains sql
begin
  declare array_length integer;
  declare return_value json;
  declare idx int;
  declare path varchar(100);

  set array_length = json_length( input_value );
  set return_value = json_array();
  set idx = 0;

  while idx < array_length do
    set path = concat( '$[', idx, ']' );
    set return_value = json_array_append
    (
      return_value,
      '$',
      json_type( json_extract( input_value, path ) )
    );

    set idx = idx + 1;
  end while;

  return return_value;
end//
delimiter ;//

delimiter //;
create procedure merge_docs
(
  in in_value json,
  inout inout_value json,
  out out_value json
)
language sql deterministic contains sql
begin
  set out_value = json_merge_preserve(in_value, inout_value);
  set inout_value = in_value;
end//
delimiter ;//


delimiter //;
create procedure merge_doc_types
(
  out in_types varchar(100),
  out inout_types varchar(100),
  out out_types varchar(100)
)
language sql deterministic contains sql
begin
  declare proc_in json;
  declare proc_inout json;
  declare proc_out json;

  set proc_in = json_array
  (
   cast( '{}' as json ),
   cast( '[]' as json ),
   'null',
   true
  );

  set proc_inout = json_array
  (
   1,
   2.3,
   timestamp( '2015-05-11 09:30:05' ),
   cast('23:24:25' as time),
   cast('2015-01-15' as date),
   b'10101'
  );

  set proc_out = null;

  call merge_docs( proc_in, proc_inout, proc_out );
  set in_types = get_types( proc_in );
  set inout_types = get_types( proc_inout );
  set out_types = get_types( proc_out );
end//
delimiter ;//

call merge_doc_types( @in_types, @inout_types, @out_types );

select @in_types, @inout_types, @out_types;

drop procedure merge_doc_types;
drop procedure merge_docs;
drop function get_types;

--echo #
--echo # Bug#20898238: WRONG RESULT FOR MAX() OF JSON SCALARS RETURNED
--echo #               WHEN NULL IS PRESENT
--echo #
CREATE TABLE bug20898238(j JSON);
INSERT INTO bug20898238 VALUES ('{"id":1}'), (NULL), ('{"id":2}'), ('{"id":0}');
SELECT MIN(JSON_EXTRACT(j, '$.id')),
       MAX(JSON_EXTRACT(j, '$.id')) FROM bug20898238;
DROP TABLE bug20898238;

--echo # ----------------------------------------------------------------------
--echo # Test of aggregate function SUM, AVG: in constrast to strings, we do not
--echo # auto-convert to numeric (double) type:
--echo # ----------------------------------------------------------------------
create table t(j json, c varchar(20));
insert into t values (cast('[1,2,3]' as json), '[a,b,c]');
insert into t values (cast(7 as json), '7'),  (cast(2 as json), '2');
--disable_warnings
select sum(j), sum(cast(j as unsigned)), sum(c) from t;
select avg(j), avg(cast(j as unsigned)), avg(c) from t;
--enable_warnings

--echo # ----------------------------------------------------------------------
--echo # Test of aggregate function COUNT(DISTINCT) and unaggregated DISTINCT
--echo # ----------------------------------------------------------------------

create table t_doc( bucket int, doc json);

insert into t_doc values
( 1, cast( 1 as json ) ),
( 1, cast( 1.0 as json ) ),
( 1, cast( 1e0 as json ) ),
( 2, cast( cast( 1 as unsigned ) as json ) ),
( 2, cast( 2 as json ) ),
( 2, cast( 2.0 as json ) ),
( 3, cast( 2e0 as json ) ),
( 3, cast( cast( 7 as unsigned ) as json ) ),
( 3, cast( 7 as json ) ),
( 4, cast( 7.0 as json ) ),
( 4, cast( 7e0 as json ) ),
( 4, cast( cast( 7 as unsigned ) as json ) ),
( 5, cast( true as json ) ),
( 5, cast( true as json ) ),
( 5, cast( false as json ) ),
( 6, cast( false as json ) ),
( 6, cast( 'null' as json ) ),
( 6, cast( 'null' as json ) ),
( 7, cast( '"abc"' as json ) ),
( 7, cast( '"abc"' as json ) ),
( 7, cast( '"abcd"' as json ) ),
( 8, cast( '"abcd"' as json ) ),
( 8, cast( '{ "a": 1, "b": 2 }' as json ) ),
( 8, cast( '{ "a": 1, "b": 2 }' as json ) ),
( 9, cast( '{ "a": 1, "b": 3 }' as json ) ),
( 9, cast( '{ "a": 1, "b": 3 }' as json ) ),
( 9, cast( '[ true, false ]' as json ) ),
( 10, cast( '[ true, false ]' as json ) ),
( 10, cast( '[ true, true ]' as json ) );

# The results depend on the order of evaluation of rows.
# Values 7, 7.0, and 7e0 compare equal for distinct but the result
# depends on which row is evaluated first so we remove .0 and e0.
--replace_regex /\.0// /e0//
select distinct( doc ) a from t_doc order by a;

select count( distinct doc ) from t_doc;
select bucket, count( distinct doc ) from t_doc group by bucket;

delete from t_doc;

create table dt(dati datetime, da date,
                tim time, ts timestamp,
                y year,
                --
                ti tinyint,   tiu tinyint unsigned,
                si smallint,  siu smallint unsigned,
                mi mediumint, miu mediumint unsigned,
                i  int,       iu  int unsigned,
                bi bigint,    biu bigint unsigned,
                boo boolean,
                --
                dc decimal(5,2),
                n numeric(5,2),
                --
                f float, d double,
                bitt bit(10),
                blb blob,
                bin binary(10),
                en enum('a','b','c'),
                se set('a','b','c'),
                --
                ge geometry,
                po point,
                ls linestring,
                py polygon,
                jso json,
                jsa json,
                id int
                );

# test with distinct values
insert into dt values('2014-11-25 18:00', '2014-11-25',
                      '18:00:59', '2014-11-25 17:00',
                      '1999',
                      --
                      127, 255,
                      32767, 65535,
                      8388607, 16777215, -- 3 bytes
                      2147483647, 4294967295, -- 4 bytes
                      9223372036854775807, 18446744073709551615,
                      true,
                                            --
                      3.1,
                      3.2,
                      --
                      3.3, 3.4,
                      b'10101',
                      '10101abcde',
                      '10001abcde',
                      'b',
                      'a,c',
                      --
                      ST_GeomFromText('POINT(1 1)'),
                      ST_GeomFromText('POINT(1 2)'),
                      ST_GeomFromText('LINESTRING(0 0,1 1,2 2)'),
                      ST_GeomFromText('POLYGON((0 0,10 0,10 10,0 10,0 0),
                                            (5 5,7 5,7 7,5 7, 5 5))'),
                      '{"a": 1, "b": 2 }',
                      '[1, 2]',
                      1
                      ),

                      ('2013-11-25 18:00', '2013-11-25',
                      '17:00:59', '2013-11-25 17:00',
                      '1998',
                      --
                      126, 254,
                      32766, 65534,
                      8388606, 16777214, -- 3 bytes
                      2147483646, 4294967294, -- 4 bytes
                      9223372036854775806, 18446744073709551614,
                      false,
                                            --
                      4.1,
                      4.2,
                      --
                      4.3, 4.4,
                      b'10111',
                      '10001abcdf',
                      '10101abcdf',
                      'a',
                      'a,b',
                      --
                      ST_GeomFromText('POINT(1 3)'),
                      ST_GeomFromText('POINT(1 4)'),
                      ST_GeomFromText('LINESTRING(0 0,1 1,2 3)'),
                      ST_GeomFromText('POLYGON((0 0,10 0,10 10,0 9,0 0),
                                            (5 5,7 5,7 7,5 7, 5 5))'),
                      '{"a": 1, "b": 3 }',
                      '[1, 3]',
                      2
                      );

# types whose representations are unstable across platforms
insert into t_doc select id, cast(f as json) from dt;
insert into t_doc select id, cast(d as json) from dt;

insert into t_doc select * from t_doc;

select count( distinct doc ) from t_doc;
select bucket, count( distinct doc ) from t_doc group by bucket;

delete from t_doc;

# types which have stable representations across platforms

insert into t_doc select id, cast(dati as json) from dt;
insert into t_doc select id, cast(da as json) from dt;
insert into t_doc select id, cast(tim as json) from dt;
insert into t_doc select id, cast(ts as json) from dt;
insert into t_doc select id, cast(y as json) from dt;

insert into t_doc select id, cast(ti as json) from dt;
insert into t_doc select id, cast(tiu as json) from dt;
insert into t_doc select id, cast(si as json) from dt;
insert into t_doc select id, cast(siu as json) from dt;
insert into t_doc select id, cast(mi as json) from dt;
insert into t_doc select id, cast(miu as json) from dt;
insert into t_doc select id, cast(i as json) from dt;
insert into t_doc select id, cast(iu as json) from dt;
insert into t_doc select id, cast(bi as json) from dt;
insert into t_doc select id, cast(biu as json) from dt;

# FIXME: booleans don't retain their boolean values. they become ints.
#insert into t_doc select id, cast(boo as json) from dt;

insert into t_doc select id, cast(dc as json) from dt;
insert into t_doc select id, cast(n as json) from dt;

insert into t_doc select id, cast(bitt as json) from dt;
insert into t_doc select id, cast(blb as json) from dt;
insert into t_doc select id, cast(bin as json) from dt;
insert into t_doc select id, cast(en as json) from dt;
insert into t_doc select id, cast(se as json) from dt;

insert into t_doc select id, cast(ge as json) from dt;
insert into t_doc select id, cast(po as json) from dt;
insert into t_doc select id, cast(ls as json) from dt;
insert into t_doc select id, cast(py as json) from dt;
insert into t_doc select id, jso from dt;
insert into t_doc select id, jsa from dt;

insert into t_doc select * from t_doc;

#The results depend on the order of evaluation of rows
select distinct( doc ) a from t_doc order by a;
select count( distinct doc ) from t_doc;
select bucket, count( distinct doc ) from t_doc group by bucket;

# test with non-distinct values

delete from t_doc;

create table ndt(dati datetime,
                ts timestamp,
                --
                ti tinyint,   tiu tinyint unsigned,
                si smallint,  siu smallint unsigned,
                mi mediumint, miu mediumint unsigned,
                i  int,       iu  int unsigned,
                bi bigint,    biu bigint unsigned,
                --
                dc decimal(5,2),
                n numeric(5,2),
                --
                f float, d double,
                id int
                );


insert into ndt values('2014-11-25 18:00',
                      '2014-11-25 18:00',
                      --
                      1, 1,
                      1, 1,
                      1, 1,
                      1, 1,
                      1, 1,
                      --
                      1.0,
                      1.0,
                      --
                      1.0, 1.0,
                      1
                      ),

                      ('2013-11-25 18:00',
                      '2013-11-25 18:00',
                      --
                      2, 2,
                      2, 2,
                      2, 2,
                      2, 2,
                      2, 2,
                      --
                      2.0,
                      2.0,
                      --
                      2.0, 2.0,
                      2
                      );

insert into t_doc select id, cast(dati as json) from ndt;
insert into t_doc select id, cast(ts as json) from ndt;

insert into t_doc select id, cast(ti as json) from ndt;
insert into t_doc select id, cast(tiu as json) from ndt;
insert into t_doc select id, cast(si as json) from ndt;
insert into t_doc select id, cast(siu as json) from ndt;
insert into t_doc select id, cast(mi as json) from ndt;
insert into t_doc select id, cast(miu as json) from ndt;
insert into t_doc select id, cast(i as json) from ndt;
insert into t_doc select id, cast(iu as json) from ndt;
insert into t_doc select id, cast(bi as json) from ndt;
insert into t_doc select id, cast(biu as json) from ndt;

insert into t_doc select id, cast(dc as json) from ndt;
insert into t_doc select id, cast(n as json) from ndt;

insert into t_doc select id, cast(f as json) from ndt;
insert into t_doc select id, cast(d as json) from ndt;

insert into t_doc select * from t_doc;

# The results depend on the order of evaluation of rows
#select distinct( doc ) a from t_doc order by a;
select count( distinct doc ) from t_doc;
select bucket, count( distinct doc ) from t_doc group by bucket;

drop table t_doc;
drop table dt;
drop table ndt;

--echo # ----------------------------------------------------------------------
--echo # Special CASTing behavior of geometry types
--echo # ----------------------------------------------------------------------

create table jtable( id int, descr varchar(20), doc json );

create table misc_dt
(
  ge geometry,
  po point,
  ls linestring,
  py polygon
);

insert into misc_dt values
(
  ST_GeomFromText('POINT(1 1)'),
  ST_GeomFromText('POINT(1 1)'),
  ST_GeomFromText('LINESTRING(0 0,1 1,2 2)'),
  ST_GeomFromText('POLYGON((0 0,10 0,10 10,0 10,0 0),
    (5 5,7 5,7 7,5 7, 5 5))')
);

insert into jtable select 1, 'geometry', cast(ge as json) from misc_dt;
insert into jtable select 2, 'point', cast(po as json) from misc_dt;
insert into jtable select 3, 'linestring', cast(ls as json) from misc_dt;
insert into jtable select 4, 'polygon', cast(py as json) from misc_dt;
#
select id, descr, json_type( doc ), doc from jtable order by id;

select json_object
(
  'geometry', ST_GeomFromText('POINT(1 1)'),
  'point', ST_GeomFromText('POINT(1 1)'),
  'linestring', ST_GeomFromText('LINESTRING(0 0,1 1,2 2)'),
  'polygon', ST_GeomFromText('POLYGON((0 0,10 0,10 10,0 10,0 0),
    (5 5,7 5,7 7,5 7, 5 5))')
);

# verify the workaround for CASTing JSON values to GEOMETRY
delete from misc_dt;
select * from misc_dt;
insert into misc_dt values
(
  (select ST_GeomFromGeoJSON( cast( doc as char ) ) from jtable where id = 1),
  (select ST_GeomFromGeoJSON( cast( doc as char ) ) from jtable where id = 2),
  (select ST_GeomFromGeoJSON( cast( doc as char ) ) from jtable where id = 3),
  (select ST_GeomFromGeoJSON( cast( doc as char ) ) from jtable where id = 4)
);
select ST_AsGeoJSON( ge ),
       ST_AsGeoJSON( po ),
       ST_AsGeoJSON( ls ),
       ST_AsGeoJSON( py )
from misc_dt;

drop table misc_dt;
drop table jtable;

create table jtable( id int, descr varchar(20), doc json );

create table misc_dt
(
  ge geometrycollection,
  po multipoint,
  ls multilinestring,
  py multipolygon
);

insert into misc_dt values
(
  geometrycollection(point(1, 1), point(2, 2)),
  multipoint(point(1, 1), point(2, 2)),
  multilinestring
  (
    linestring(point(0, 0), point(1, 1), point(2, 2)),
    linestring(point(0, 0), point(11, 11), point(12, 12))
  ),
  multipolygon
  (
      polygon
      (
        linestring(point(0, 0), point(10, 0), point(10, 10), point(0, 10), point(0, 0)),
        linestring(point(5, 5), point(7, 5), point(7, 7), point(5, 7), point(5, 5))
      ),
      polygon
      (
        linestring(point(0, 0), point(10, 0), point(10, 10), point(0, 10), point(0, 0)),
        linestring(point(5, 5), point(7, 5), point(7, 7), point(5, 7), point(5, 5))
      )
  )
);

insert into jtable select 1, 'geometrycollection', cast(ge as json) from misc_dt;
insert into jtable select 2, 'multipoint', cast(po as json) from misc_dt;
insert into jtable select 3, 'multilinestring', cast(ls as json) from misc_dt;
insert into jtable select 4, 'multipolygon', cast(py as json) from misc_dt;
#
select id, descr, json_type( doc ), doc from jtable order by id;

select ST_AsGeoJSON( ge ),
       ST_AsGeoJSON( po ),
       ST_AsGeoJSON( ls ),
       ST_AsGeoJSON( py )
from misc_dt;

delete from misc_dt;
select * from misc_dt;
insert into misc_dt values
(
  (select ST_GeomFromGeoJSON( cast( doc as char ) ) from jtable where id = 1),
  (select ST_GeomFromGeoJSON( cast( doc as char ) ) from jtable where id = 2),
  (select ST_GeomFromGeoJSON( cast( doc as char ) ) from jtable where id = 3),
  (select ST_GeomFromGeoJSON( cast( doc as char ) ) from jtable where id = 4)
);
select ST_AsGeoJSON( ge ),
       ST_AsGeoJSON( po ),
       ST_AsGeoJSON( ls ),
       ST_AsGeoJSON( py )
from misc_dt;

drop table misc_dt;
drop table jtable;

--echo # ----------------------------------------------------------------------
--echo # Test of COALESCE
--echo # ----------------------------------------------------------------------

select coalesce(cast(1 as json), cast(2 as json));
--sorted_result
select j, coalesce(j, cast(3 as json)) from t;
--sorted_result
select j, coalesce(j, 666) from t;
--sorted_result
select j, json_type(coalesce(j, '[1,2,3]')) from t;
--sorted_result
select j, json_type(coalesce(j, 'abc')) from t;
--sorted_result
select j, json_type(coalesce(j, cast('"arg2"' as json))) from t;
--sorted_result
select j, json_type(coalesce(j, j)) from t;
--echo inconsistent result: error message depends on the order of evaluation of rows
--echo --error ER_INVALID_JSON_TEXT_IN_PARAM
--echo select json_type(coalesce(json_extract(concat(j, 'abc'), '\$'), j)) from t;
--echo --error ER_INVALID_JSON_TEXT_IN_PARAM
--echo #select json_type(coalesce(t1.j, json_extract(concat(t2.j, 'abc'), '\$')))
--echo from t t1, t t2;

drop table t;

--echo # ----------------------------------------------------------------------
--echo # Auto-convert of non-utf8 returning system function
--echo # ----------------------------------------------------------------------
create table t(j json, id int);
insert into t values ('{"user": "foo"}', 8), (NULL, 8);
update t set j=json_set(j, '$.user', current_user()) where id=8;
select j from t order by j;
update t set j=json_set(j, '$.user', rtrim('foo    '))  where id=8;
select j from t order by j;
update t set j=json_set(j, '$.user', hex('abc'))  where id=8;
select j from t order by j;
update t set j=json_set(j, '$.user', md5('bingle'))  where id=8;
select j from t order by j;
update t set j=json_set(j, '$.user', database())  where id=8;
select j from t order by j;
update t set j=json_set(j, '$.user', schema()) where id=8;
select j from t order by j;
#
# The hex of some UTF-8 from supplementary plane: U+2070E
update t set j=json_set(j, '$.user',
  cast(UNHEX('F0A09C8E') as char character set 'utf8mb4')) where id=8;
set names 'utf8mb4'; # se we can see the character
select j from t order by j;
select char_length(json_extract(j, '$.user')) from t order by j;
drop table t;


--echo #
--echo # Bug#21257946 JSON_TYPE(TEXT) OF TABLE COLUMN STICKS WITH NULL
--echo #              AFTER FIRST ENCOUNTER OF NULL
--echo #
CREATE TABLE T_WITH_NULLS(i INT, j JSON);
INSERT INTO T_WITH_NULLS VALUES
(0, NULL),
(1, '[1]'),
(2, NULL),
(3, '{"a":"b"}'),
(4, NULL),
(5, '"abc"');
let $query= SELECT
JSON_VALID(j),
JSON_TYPE(j),
JSON_KEYS(j),
JSON_EXTRACT(j, '\$'),
JSON_REMOVE(j, '\$.a.b.c'),
JSON_ARRAY_APPEND(j, '\$', 2),
JSON_SET(j, '\$[0]', 2),
JSON_INSERT(j, '\$[0]', 2),
JSON_REPLACE(j, '\$[0]', 2),
JSON_MERGE_PRESERVE(j, j),
JSON_SEARCH(j, 'one', 'abc'),
JSON_CONTAINS(j, '[1]'),
JSON_CONTAINS_PATH(j, 'all', '\$.a'),
JSON_LENGTH(j),
JSON_DEPTH(j),
JSON_ARRAY(j, j),
JSON_OBJECT('k', j),
JSON_UNQUOTE(CAST(j AS CHAR)),
JSON_QUOTE(CAST(j AS CHAR)),
JSON_PRETTY(j),
JSON_STORAGE_FREE(j),
JSON_STORAGE_SIZE(j)
FROM T_WITH_NULLS
ORDER BY i;
eval $query;
# It should work the same way with a TEXT column as with a JSON column.
ALTER TABLE T_WITH_NULLS MODIFY COLUMN j TEXT;
eval $query;
DROP TABLE T_WITH_NULLS;

# Make sure that every JSON function accepts latin1 text arguments. The JSON
# functions use utf8mb4 internally, so they will need to perform charset
# conversion.
CREATE TABLE t_latin1(id INT PRIMARY KEY AUTO_INCREMENT,
                      json_text VARCHAR(20),
                      json_atom_text VARCHAR(20),
                      json_path VARCHAR(20))
CHARACTER SET 'latin1';
INSERT INTO t_latin1 (json_text, json_atom_text, json_path) VALUES
(CONVERT(X'5B22E6F8E5225D' USING latin1),             # ["\u00e6\u00f8\u00e5"]
 CONVERT(X'E5F8E6' USING latin1),                     # \u00e5\u00f8\u00e6
 '$[0]'),
(CONVERT(X'7B22E6F8E5223A22E6F8E5227D' USING latin1),
                                  # {"\u00e6\u00f8\u00e5":"\u00e6\u00f8\u00e5"}
 CONVERT(X'E5F8E6' USING latin1),                     # \u00e5\u00f8\u00e6
 CONVERT(X'242E22E6F8E522' USING latin1));            # $."\u00e6\u00f8\u00e5"
SELECT * FROM t_latin1 ORDER BY id;
SELECT CAST(json_text AS JSON) FROM t_latin1 ORDER BY id;
SELECT JSON_VALID(json_text) FROM t_latin1 ORDER BY id;
SELECT JSON_VALID(json_atom_text) FROM t_latin1 ORDER BY id;
SELECT JSON_TYPE(json_text) FROM t_latin1 ORDER BY id;
SELECT JSON_EXTRACT(json_text, json_path) FROM t_latin1 ORDER BY id;
SELECT JSON_REMOVE(json_text, json_path) FROM t_latin1 ORDER BY id;
SELECT JSON_ARRAY_APPEND(json_text, json_path, json_atom_text)
FROM t_latin1 ORDER BY id;
SELECT JSON_SET(json_text, json_path, json_atom_text) FROM t_latin1 ORDER BY id;
SELECT JSON_INSERT(json_text, json_path, json_atom_text)
FROM t_latin1 ORDER BY id;
SELECT JSON_REPLACE(json_text, json_path, json_atom_text)
FROM t_latin1 ORDER BY id;
SELECT JSON_MERGE_PRESERVE(json_text, json_text) FROM t_latin1 ORDER BY id;
SELECT JSON_SEARCH(json_text, CONVERT('one' USING latin1), json_atom_text,
                  CONVERT(X'F8' USING latin1), json_path)
FROM t_latin1 ORDER BY id;
SELECT JSON_CONTAINS(json_text, json_text, json_path) FROM t_latin1 ORDER BY id;
SELECT JSON_CONTAINS_PATH(json_text, CONVERT('one' USING latin1), json_path)
FROM t_latin1 ORDER BY id;
SELECT JSON_LENGTH(json_text, json_path) FROM t_latin1 ORDER BY id;
SELECT JSON_DEPTH(json_text) FROM t_latin1 ORDER BY id;
SELECT JSON_ARRAY(json_atom_text, json_atom_text) FROM t_latin1 ORDER BY id;
SELECT JSON_OBJECT(json_atom_text, json_atom_text) FROM t_latin1 ORDER BY id;
SELECT JSON_UNQUOTE(json_atom_text) FROM t_latin1 ORDER BY id;
SELECT JSON_UNQUOTE(CONVERT(CONCAT('"', json_atom_text, '"') USING latin1))
FROM t_latin1 ORDER BY id;
SELECT JSON_QUOTE(json_atom_text) FROM t_latin1 ORDER BY id;
DROP TABLE t_latin1;

--echo # ----------------------------------------------------------------------
--echo # Test that boolean expressions are treated as boolean atom literals
--echo # ----------------------------------------------------------------------

create table t_bool_literals( a int, b varchar(10) );
insert into t_bool_literals values ( 1, 'food' ), ( 2, 'fool' ), ( 3, 'water' );

# expressions built out of logical connectives should evaluate to boolean literals, but they don't
select a, json_array( ((a < 3) and (a > 1)) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', ((a < 3) and (a > 1)) ) from t_bool_literals order by a;

select a, json_array( not ((a < 3) and (a > 1)) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', not ((a < 3) and (a > 1)) ) from t_bool_literals order by a;

select a, json_array( ((a < 3) or (a > 1)) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', ((a < 3) or (a > 1)) ) from t_bool_literals order by a;

select a, json_array( not ((a < 3) or (a > 1)) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', not ((a < 3) or (a > 1)) ) from t_bool_literals order by a;

select json_array( not true, not false );
select json_array_append( '[]', '$', not true, '$', not false );

select a, json_array( 1 and true ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', 1 and true ) from t_bool_literals order by a;

select a, json_array( not 1 ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', not 1 ) from t_bool_literals order by a;

# true and false literals
select json_array( true, false );
select json_array_append( '[]', '$', true, '$', false );

# comparison operators should evaluate to boolean literals
select a, json_array( (a < 3) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', (a < 3) ) from t_bool_literals order by a;

select a, json_array( (a <= 3) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', (a <= 3) ) from t_bool_literals order by a;

select a, json_array( (a > 3) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', (a > 3) ) from t_bool_literals order by a;

select a, json_array( (a >= 3) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', (a >= 3) ) from t_bool_literals order by a;

select a, json_array( (a <> 3) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', (a <> 3) ) from t_bool_literals order by a;

select a, json_array( (a != 3) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', (a != 3) ) from t_bool_literals order by a;

# IS NULL and IS NOT NULL
select a, json_array( a is null ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', a is null ) from t_bool_literals order by a;

select a, json_array( a is not null ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', a is not null ) from t_bool_literals order by a;

# IS TRUE and IS NOT TRUE

select a, json_array( a is true ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', a is true ) from t_bool_literals order by a;

select a, json_array( a is not true ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', a is not true ) from t_bool_literals order by a;

# NULLIF which coalesce booleans should evaluate to boolean literals
select a, json_array(nullif(true, false)  ) from t_bool_literals order by a;
select a, json_array_append
(
  '[]',
  '$',  nullif(true, false)
) from t_bool_literals order by a;

# would be nice if CASE coalesced a boolean type if all branches are boolean. FIXME maybe
#select a, json_array( case when (a > 1) then true else false end ) from t_bool_literals order by a;
#select a, json_array_append
#(
#  '[]',
#  '$', case when (a > 1) then true else false end
#) from t_bool_literals order by a;

# as a workaround, you can always AND problematic expressions with true
select a, json_array( (case when (a > 1) then true else false end) and true ) from t_bool_literals order by a;
select a, json_array_append
(
  '[]',
  '$', (case when (a > 1) then true else false end) and true
) from t_bool_literals order by a;

# between predicates should evaluate to boolean literals
select a, json_array( a between 2 and 4 ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', a between 2 and 4 ) from t_bool_literals order by a;

# in predicates should evaluate to boolean literals
select a, json_array( a in (1,3) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', a in (1,3) ) from t_bool_literals order by a;

# like predicates should evaluate to boolean literals
select a, json_array( b like 'foo%' ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', b like 'foo%' ) from t_bool_literals order by a;

# regexp predicates should evaluate to boolean literals
select a, json_array( b REGEXP '^fo+d' ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', b REGEXP '^fo+d' ) from t_bool_literals order by a;

select a, json_array( b rlike '^fo+d' ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', b rlike '^fo+d' ) from t_bool_literals order by a;

select a, json_array( b not REGEXP '^fo+d' ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', b not REGEXP '^fo+d' ) from t_bool_literals order by a;

select a, json_array( b not rlike '^fo+d' ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', b not rlike '^fo+d' ) from t_bool_literals order by a;

# quantified comparisons should evaluate to boolean literals
select a, json_array( b = some( select b from t_bool_literals ) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', b = some( select b from t_bool_literals ) ) from t_bool_literals order by a;

select a, json_array( b = all( select b from t_bool_literals ) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', b = all( select b from t_bool_literals ) ) from t_bool_literals order by a;

select a, json_array( b = any( select b from t_bool_literals ) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', b = any( select b from t_bool_literals ) ) from t_bool_literals order by a;

select a, json_array( b > some( select b from t_bool_literals ) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', b > some( select b from t_bool_literals ) ) from t_bool_literals order by a;

select a, json_array( b > all( select b from t_bool_literals ) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', b > all( select b from t_bool_literals ) ) from t_bool_literals order by a;

select a, json_array( b > any( select b from t_bool_literals ) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', b > any( select b from t_bool_literals ) ) from t_bool_literals order by a;

select a, json_array( b < some( select b from t_bool_literals ) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', b < some( select b from t_bool_literals ) ) from t_bool_literals order by a;

select a, json_array( b < all( select b from t_bool_literals ) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', b < all( select b from t_bool_literals ) ) from t_bool_literals order by a;

select a, json_array( b < any( select b from t_bool_literals ) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', b < any( select b from t_bool_literals ) ) from t_bool_literals order by a;

select a, json_array( b <= some( select b from t_bool_literals ) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', b <= some( select b from t_bool_literals ) ) from t_bool_literals order by a;

select a, json_array( b <= all( select b from t_bool_literals ) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', b <= all( select b from t_bool_literals ) ) from t_bool_literals order by a;

select a, json_array( b <= any( select b from t_bool_literals ) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', b <= any( select b from t_bool_literals ) ) from t_bool_literals order by a;

select a, json_array( b >= some( select b from t_bool_literals ) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', b >= some( select b from t_bool_literals ) ) from t_bool_literals order by a;

select a, json_array( b >= all( select b from t_bool_literals ) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', b >= all( select b from t_bool_literals ) ) from t_bool_literals order by a;

select a, json_array( b >= any( select b from t_bool_literals ) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', b >= any( select b from t_bool_literals ) ) from t_bool_literals order by a;

# exists predicates should evaluate to boolean literals
select a, json_array( exists( select b from t_bool_literals where a = 1 ) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', exists( select b from t_bool_literals where a = 1 ) ) from t_bool_literals order by a;

select a, json_array( not exists( select b from t_bool_literals where a = 1 ) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', not exists( select b from t_bool_literals where a = 1 ) ) from t_bool_literals order by a;

# json_valid() calls should evaluate to boolean literals
select a, json_array( json_valid( b ) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', json_valid( b ) ) from t_bool_literals order by a;

select a, json_array( not json_valid( b ) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$', not json_valid( b ) ) from t_bool_literals order by a;

# json_contains_path() calls should evaluate to boolean literals
select json_array( json_contains_path( '{ "a" : { "b" : 100 } }', 'all', '$.a.b' ) );

# gtid_subset() calls should evaluate to boolean literals
select a, json_array( gtid_subset('3E11FA47-71CA-11E1-9E33-C80AA9429562:23', '3E11FA47-71CA-11E1-9E33-C80AA9429562:21-57') )
from t_bool_literals order by a;
select a, json_array_append( '[]', '$', gtid_subset('3E11FA47-71CA-11E1-9E33-C80AA9429562:23', '3E11FA47-71CA-11E1-9E33-C80AA9429562:21-57') )
from t_bool_literals order by a;

select a, json_array( not gtid_subset('3E11FA47-71CA-11E1-9E33-C80AA9429562:23', '3E11FA47-71CA-11E1-9E33-C80AA9429562:21-57') )
from t_bool_literals order by a;
select a, json_array_append( '[]', '$', not gtid_subset('3E11FA47-71CA-11E1-9E33-C80AA9429562:23', '3E11FA47-71CA-11E1-9E33-C80AA9429562:21-57') )
from t_bool_literals order by a;

# comparisons to subqueries should evaluate to boolean literals
select a, json_array( b = ( select distinct b from t_bool_literals where a = 1 ) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$',  b = ( select distinct b from t_bool_literals where a = 1 ) ) from t_bool_literals order by a;

select a, json_array( b > ( select distinct b from t_bool_literals where a = 1 ) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$',  b > ( select distinct b from t_bool_literals where a = 1 ) ) from t_bool_literals order by a;

select a, json_array( b >= ( select distinct b from t_bool_literals where a = 1 ) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$',  b >= ( select distinct b from t_bool_literals where a = 1 ) ) from t_bool_literals order by a;

select a, json_array( b < ( select distinct b from t_bool_literals where a = 1 ) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$',  b < ( select distinct b from t_bool_literals where a = 1 ) ) from t_bool_literals order by a;

select a, json_array( b <= ( select distinct b from t_bool_literals where a = 1 ) ) from t_bool_literals order by a;
select a, json_array_append( '[]', '$',  b <= ( select distinct b from t_bool_literals where a = 1 ) ) from t_bool_literals order by a;

# make sure ordinary subselects still function correctly
select a, json_array( ( select distinct a from t_bool_literals where a = 1 ) ) from t_bool_literals order by a;

drop table t_bool_literals;

--echo # ----------------------------------------------------------------------
--echo # Verify that all of the string types behave similarly when used as ANY_JSON_ATOMS
--echo # ----------------------------------------------------------------------

create table t_char( a int, b char(20) );
insert into t_char values ( 1, 'foo' );

create table t_varchar( a int, b varchar(20) );
insert into t_varchar values ( 1, 'foo' );

Create table t_tinytext( a int, b tinytext );
insert into t_tinytext values ( 1, 'foo' );

create table t_text( a int, b text );
insert into t_text values ( 1, 'foo' );

create table t_mediumtext( a int, b mediumtext );
insert into t_mediumtext values ( 1, 'foo' );

create table t_longtext( a int, b longtext );
insert into t_longtext values ( 1, 'foo' );

# treated as a string. evaluates to ["foo"]
select json_array( b ) from t_char;
select json_array( b ) from t_varchar;
select json_array( b ) from t_tinytext;
select json_array( b ) from t_text;
select json_array( b ) from t_mediumtext;
select json_array( b ) from t_longtext;

# casts to CHAR should still be strings
select json_array( cast( b as char ) ) from t_char;
select json_array( cast( b as char ) ) from t_varchar;
select json_array( cast( b as char ) ) from t_tinytext;
select json_array( cast( b as char ) ) from t_text;
select json_array( cast( b as char ) ) from t_mediumtext;
select json_array( cast( b as char ) ) from t_longtext;

# string-valued XML functions should behave as strings when used as ANY_JSON_ATOMs
select json_array( UpdateXML('<a><b>ccc</b><d></d></a>', '/a/d', '<e>fff</e>') );
select json_array( cast( UpdateXML('<a><b>ccc</b><d></d></a>', '/a/d', '<e>fff</e>') as char ) );
select json_array( ExtractValue('<r><n id="1">v1</n><n id="2">v2</n></r>','//n[@id=1]' ) );
select json_array( cast( ExtractValue('<r><n id="1">v1</n><n id="2">v2</n></r>','//n[@id=1]' ) as char ) );

drop table t_char;
drop table t_varchar;
drop table t_tinytext;
drop table t_text;
drop table t_mediumtext;
drop table t_longtext;

--echo # ----------------------------------------------------------------------
--echo # Check that JSON values stemming from views and derived tables work
--echo # ----------------------------------------------------------------------
create table t(x int);
insert into t values (NULL), (4);
select json_array(x) from (select x from t) tt order by x;
create view v as select * from t;
select json_array(x) from v order by x;

drop view v;
drop table t;

--echo # ----------------------------------------------------------------------
--echo # Ignore collation.collation when handing off val_str to a JSON field -
--echo # bug found by John E.
--echo # ----------------------------------------------------------------------
create table t3( col_json json );
insert into t3(col_json) values ( json_quote( '1' ) );
select * from t3;
select json_type(col_json) from t3;

drop table t3;

--echo # ----------------------------------------------------------------------
--echo # Correctly escape key names when pretty-printing JSON objects.
--echo # Correct behavior means that the strings can be re-used for
--echo # their original purposes as key names and paths.
--echo # ----------------------------------------------------------------------

create table jep( key_col int primary key, doc json, path varchar( 50 ) );
insert into jep values
( 1, '{ "one \\"potato": "seven"  }', '$."one \\"potato"' ),
( 2, '{ "one \\npotato": "seven"  }', '$."one \\npotato"' ),
( 3, '{ "one \\tpotato": "seven"  }', '$."one \\tpotato"' ),
( 4, '{ "one \\bpotato": "seven"  }', '$."one \\bpotato"' ),
( 5, '{ "one \\fpotato": "seven"  }', '$."one \\fpotato"' ),
( 6, '{ "one \\rpotato": "seven"  }', '$."one \\rpotato"' ),
( 7, '{ "one \\\\potato": "seven"  }', '$."one \\\\potato"' );

insert into jep select key_col + 100, cast( doc as char ), path from jep;

select key_col, doc, json_keys( doc ) from jep order by key_col;

select key_col, doc, json_extract( doc, cast(path as char) ) from jep order by key_col;

select * from jep order by key_col;

drop table jep;

--echo # ----------------------------------------------------------------------
--echo # Test that cached, constant path objects are restored
--echo # after the leg popping which happens inside json_insert()
--echo # and json_replace().
--echo # ----------------------------------------------------------------------

create table t_cache( id int, doc json );

insert into t_cache values
( 1, '{ "a": { "b": 1 } }' ),
( 2, '{ "a": { "c": 1 } }' ),
( 3, '{ "a": { "d": 1 } }' );

select id, doc, json_insert( doc, '$.a.c', 2 ) from t_cache order by id;
select id, doc, json_insert( doc, '$.a.c', 2, '$.a.d', 3 ) from t_cache order by id;

delete from t_cache;

insert into t_cache values
( 1, '{ "a": { "b": 1, "c": 2, "d": 3 } }' ),
( 2, '{ "a": { "c": 2, "d": 3 } }' ),
( 3, '{ "a": { "b": 1, "d": 3 } }' ),
( 4, '{ "a": { "b": 1, "c": 2 } }' ),
( 5, '{ "a": { "b": 1 } }' ),
( 6, '{ "a": { "c": 2 } }' ),
( 7, '{ "a": { "d": 3 } }' ),
( 8, '{ "a": {} }' );

select id, doc, json_replace( doc, '$.a.c', 20 ) from t_cache order by id;
select id, doc, json_replace( doc, '$.a.c', 20, '$.a.d', 30 ) from t_cache order by id;

drop table t_cache;

--echo # ----------------------------------------------------------------------
--echo # Test that one_or_all arguments are cached correctly.
--echo # ----------------------------------------------------------------------

create table t_ooa( id int, doc json, one_or_all varchar(10) );

insert into t_ooa values
( 1, '{ "a": 1, "b": 2, "c": 3 }', 'one' ),
( 2, '{ "d": 4 }', 'one' ),
( 3, '{ "a": 1, "b": 2, "d": 4 }', 'all' ),
( 4, '{ "a": 1, "c": 3 }', 'all' ),
( 5, '{ "d": 4 }', 'all' ),
( 6, '{ "a": 1, "b": 2, "c": 3 }', null );

select id, doc, one_or_all, json_contains_path( doc, one_or_all, '$.a', '$.b' ) from t_ooa order by id;
select id, doc, json_contains_path( doc, 'one', '$.a', '$.b' ) from t_ooa order by id;
select id, doc, json_contains_path( doc, 'all', '$.a', '$.b' ) from t_ooa order by id;
select id, doc, json_contains_path( doc, null, '$.a', '$.b' ) from t_ooa order by id;

delete from t_ooa;

insert into t_ooa values
( 1, '{ "a": "foot", "b": "fool", "c": "food" }', 'one' ),
( 1, '{ "a": "foot", "b": "fool", "c": "food" }', 'all' ),
( 1, '{ "a": "foot", "b": "fool", "c": "food" }', null );

select id, doc, one_or_all, json_search( doc, one_or_all, 'foo%' ) from t_ooa order by id;
select id, doc, json_search( doc, 'one', 'foo%' ) from t_ooa order by id;
select id, doc, json_search( doc, 'all', 'foo%' ) from t_ooa order by id;
select id, doc, json_search( doc, null, 'foo%' ) from t_ooa order by id;

drop table t_ooa;

# This test case reproduces a problem seen during development. The update
# statement crashed if the target table was the inner table of the join.
CREATE TABLE t1(j JSON);
CREATE TABLE t2(j JSON);
INSERT INTO t1 VALUES ('[1]'), ('[2]'), ('[3]'), ('[4]');
INSERT INTO t2 VALUES ('[1]');
ANALYZE TABLE t1, t2;
let $query=
UPDATE t1, t2 SET t1.j = JSON_INSERT(t2.j, '\$[1]', t2.j) WHERE t1.j=t2.j;
eval EXPLAIN $query;
eval $query;
SELECT * FROM t1 ORDER BY (CAST(j AS CHAR));
DROP TABLE t1, t2;

--echo #
--echo # Bug#20888919: ASSERT `!THD->IS_ERROR()' FAILED IN HANDLE_QUERY()
--echo #               ON EXPLAIN SELECT JSON
--echo #
create table t (pk int primary key, col_json json);
ANALYZE TABLE t;
explain SELECT col_json FROM t WHERE pk = 1;
drop table t;

--echo # ----------------------------------------------------------------------
--echo # Bug#20889248 Used to crash the server
--echo # ----------------------------------------------------------------------
create table tt(i int, j json, si int);
select count(*) , json_keys('{"key17": {"a": {"b": "c"}}, "key88": "value94"}');

# Tests Item_copy_json::save_in_field. int target column here gets assigned via
# JSON->string->int parse since Field_long::store doesn't have an overload for
# JSON. Similar for other non-JSON target columns. The JSON column assignment
# does not go via string, since Field_json knows how to store JSON.
insert into tt(i, j)
  select count(*), json_extract('{"key17": {"a": {"b": "c"}}, "key88": 100}',
                               '$.key88');
insert into tt(i, si)
  select count(*), json_extract('{"key17": {"a": {"b": "c"}}, "key88": 100}',
                               '$.key88');
select * from tt order by i;

# This exercises Item_copy_json::val_real
delete from tt;
insert into tt(j) values (cast(1 as json)), (null);
select sum( distinct j ) from tt group by j having j in ( avg( 1 ), 1 + j);

# Exercise Item_copy_json::val_json
SELECT JSON_ARRAY(j), COUNT(*) FROM tt GROUP BY j, i WITH ROLLUP;

# Exercise Item_copy_json::val_int
SELECT REPEAT('abc', j), COUNT(*) FROM tt GROUP BY j, i WITH ROLLUP;

# Exercise Item_copy_json::val_str
SELECT REPEAT(j, 2), COUNT(*) FROM tt GROUP BY j, i WITH ROLLUP;

# Exercise Item_copy_json::val_decimal
SELECT CAST(j AS DECIMAL(5,2)), COUNT(*) FROM tt GROUP BY j, i WITH ROLLUP;

# Exercise Item_copy_json::get_time
UPDATE tt SET j = CAST(CAST('12:13:14' AS TIME) AS JSON) WHERE j IS NOT NULL;
SELECT CAST(j AS TIME), COUNT(*) FROM tt GROUP BY j, i WITH ROLLUP;

# Exercise Item_copy_json::get_date
SELECT CAST(j AS DATE) = CURRENT_DATE, COUNT(*) FROM tt
GROUP BY j, i WITH ROLLUP;
UPDATE tt SET j = CAST(CAST('2015-06-19' AS DATE) AS JSON) WHERE j IS NOT NULL;
SELECT CAST(j AS DATE), COUNT(*) FROM tt GROUP BY j, i WITH ROLLUP;

# Exercise an error path through Item_copy_json::val_str
DELETE FROM tt;
INSERT INTO tt(j) VALUES (JSON_ARRAY(REPEAT('abc', 100)));
UPDATE tt SET j = JSON_ARRAY(j,j,j,j);
SET GLOBAL net_buffer_length = 1024;
SET GLOBAL max_allowed_packet = 1024;
CONNECT (con1,localhost,root,,);
CONNECTION con1;
SELECT REPEAT(j, 2), COUNT(*) FROM tt GROUP BY j, i WITH ROLLUP;
CONNECTION default;
DISCONNECT con1;
SET GLOBAL max_allowed_packet = default;
SET GLOBAL net_buffer_length = default;

DROP TABLE tt;

--echo # ----------------------------------------------------------------------
--echo # Bug#20914054 Used to crash the server
--echo # ----------------------------------------------------------------------
CREATE TABLE t1 (
  pk INT NOT NULL,
  col_int_key INT,
  col_json json,
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key)
);

INSERT INTO t1 VALUES (8, 4, '{}');

CREATE TABLE t2 (
  pk INT NOT NULL,
  PRIMARY KEY (pk)
);

INSERT INTO t2 VALUES (20);

SELECT MIN(JSON_KEYS( t1.col_json )) AS field1
FROM t1 JOIN t2
HAVING field1 = 7;

drop table t1;
drop table t2;

--echo # ----------------------------------------------------------------------
--echo # Bug#20920788 Used to give SQL state 22032: Cannot create a JSON value
--echo # from a string with CHARACTER SET 'binary'.
--echo #----------------------------------------------------------------------

CREATE TABLE t (
  col_json JSON,
  col_varchar VARCHAR(1),
  col_varchar_key VARCHAR(1),
  KEY col_varchar_key (col_varchar_key)
);

INSERT INTO t VALUES ('{}', 'a', 'a');

--echo # This always succeeded, group by column is indexed, optimizer does not
--echo # use filesort:
SELECT MAX(col_json) AS field1, col_varchar_key AS field2 FROM t GROUP BY field2;

--echo # This used to fail, group by column is not indexed, EXPLAIN says
--echo # filesort is used:
SELECT MAX(col_json) AS field1, col_varchar AS field2 FROM t GROUP BY field2;

drop table t;

--echo # ----------------------------------------------------------------------
--echo # Bug#20962317 WARNING 3150 'INVALID JSON VALUE FOR CAST TO INTEGER' ON
--echo #              SUBQUERY IN JSON_VALID
--echo #----------------------------------------------------------------------
create table myt(col_json json);
insert into myt values ('{}');
--echo # This statement used to give two wrong warnings
select json_valid((select col_json from myt));
drop table myt;

--echo # ----------------------------------------------------------------------
--echo # Bug#20954309 JSON_SEARCH() IN VIEWS DOES NOT WORK, ALWAYS RETURNS NULL
--echo #----------------------------------------------------------------------

CREATE TABLE t_20954309 (id int, col_json JSON);
INSERT INTO t_20954309 VALUES
  (2, '{"keyA": "eleven"}');
CREATE VIEW v1_20954309 AS SELECT id, JSON_SEARCH(col_json, 'one', 'ele%' ) FROM t_20954309;

CREATE VIEW v2_20954309 AS SELECT id, col_json FROM t_20954309;


SELECT id, JSON_SEARCH(col_json, 'one', 'ele%' ) from t_20954309 order by id;
SELECT id, JSON_SEARCH(col_json, 'one', 'eleven' ) from v2_20954309 order by id;

SELECT * FROM v1_20954309 order by id;

drop view v1_20954309;
drop view v2_20954309;
drop table t_20954309;

#
# Arguments vary from row to row.
#
create table t_20954309 (id int, doc JSON, search_string varchar(20), escape_char varchar(10) );
insert into t_20954309 values
  (1, '{"match11": "eleven", "match12": "element", "notMatch": "elven" }', 'ele%', null ),
  (2, '{"match21": "eleven", "match22": "element", "notMatch": "elven" }', 'ele%', 'z' ),
  (3, '{"match31": "tw%elve", "match32": "tw%ilight", "notMatch": "twitter" }', 'tw|%%', '|' );

select id, json_search( doc, 'all', search_string, '|' ) from t_20954309 order by id;

create view v_20954309 as select id, json_search( doc, 'all', search_string, '|' ) from t_20954309 order by id;
select * from v_20954309;

select id, json_search( doc, 'all', search_string, null ) from t_20954309 where id < 3 order by id;
create view v2_20954309 as select id, json_search( doc, 'all', search_string, null ) result from t_20954309 where id < 3 order by id;
select * from v2_20954309;

drop view v_20954309;
drop view v2_20954309;
drop table t_20954309;

create table t_doc (id int, doc JSON );
insert into t_doc values
  (1, '{"match11": "eleven", "match12": "element", "notMatch": "elven" }' ),
  (2, '{"match21": "eleven", "match22": "element", "notMatch": "elven" }' ),
  (3, '{"match31": "tw%elve", "match32": "tw%ilight", "notMatch": "twitter" }' );

create table t_search_string (id int, search_string varchar(20) );
insert into t_search_string values
  (1, 'ele%' ),
  (2, 'ele%' ),
  (3, 'tw|%%' );

select t.id, json_search( doc, 'all', (select search_string from t_search_string s where s.id = t.id), '|' )
from t_doc t order by id;

create view v_doc as
select t.id, json_search( doc, 'all', (select search_string from t_search_string s where s.id = t.id), '|' )
from t_doc t order by id;

select * from v_doc;

drop view v_doc;
drop table t_doc;
drop table t_search_string;

set names default;

-- echo #
-- echo # Bug#20972793 ASSERT FIELD_TYPE() == MYSQL_TYPE_JSON...
-- echo #              IN ARG_COMPARATOR::COMPARE_JSON
-- echo #
CREATE TABLE t1 (
  pk INT NOT NULL,
  col_int_key INT,
  col_int INT,
  col_json JSON,
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key)
);
INSERT INTO t1 VALUES (2,4,2,NULL);
CREATE TABLE t2 (
  pk INT NOT NULL,
  col_int_key INT,
  PRIMARY KEY (pk),
  KEY col_int_key (col_int_key)
);
SELECT
  (SELECT MAX(sq1_alias1.pk) AS sq1_field1
   FROM (t1 AS sq1_alias1
     INNER JOIN t2 AS sq1_alias2
     ON (sq1_alias2.col_int_key = sq1_alias1.col_int_key)
   )
   WHERE sq1_alias2.pk <= alias1.col_int
  ) AS field1,
  MAX(alias1.col_json) AS field2
FROM (
  SELECT sq2_alias1.*
  FROM t1 AS sq2_alias1
) AS alias1
GROUP BY field1
HAVING field2 > 1;
DROP TABLE t1, t2;

--echo # ----------------------------------------------------------------------
--echo # Bug#20987329 VALUE OF PREPARED STATEMENT PLACEHOLDER FOR PARAMETER
--echo #              IN JSON_EXTRACT IS STICKY
--echo #----------------------------------------------------------------------

# should get different results with different parameter values

# json_extract()

CREATE TABLE t_reuse (pk INT, col_json JSON);
INSERT INTO t_reuse VALUES (1, '{"keyA": 1}'), (2, '{"keyA": 2, "keyB": 22}');

PREPARE getjson FROM 'SELECT JSON_EXTRACT(col_json, ?) FROM t_reuse order by pk';
SET @mypath = '$.keyA';
EXECUTE getjson USING @mypath;
SET @mypath = '$.keyB';
EXECUTE getjson USING @mypath;

drop table t_reuse;

--echo #
--echo # Test that max_allowed_packet is respected.
--echo #
SET GLOBAL net_buffer_length = 1024;
SET GLOBAL max_allowed_packet = 1024;
CONNECT (con1,localhost,root,,);
CONNECTION con1;
CREATE TABLE t1(j JSON);
INSERT INTO t1 VALUES (JSON_ARRAY(REPEAT('abc', 100)));
SELECT JSON_ARRAY(j, j, j, j) FROM t1;
--error ER_WARN_ALLOWED_PACKET_OVERFLOWED
UPDATE t1 SET j = JSON_ARRAY(j, j, j, j);
CREATE TABLE t2(s TEXT);
--error ER_WARN_ALLOWED_PACKET_OVERFLOWED
INSERT INTO t2 SELECT JSON_ARRAY(j, j, j, j) FROM t1;
SELECT * FROM t2;
INSERT INTO t2 SELECT * FROM t1;
--error ER_WARN_ALLOWED_PACKET_OVERFLOWED
UPDATE t2 SET s = JSON_ARRAY(s, s, s, s);
DROP TABLE t1, t2;
CONNECTION default;
DISCONNECT con1;
SET GLOBAL max_allowed_packet = default;
SET GLOBAL net_buffer_length = default;

--echo #
--echo # Test that very deep documents are rejected.
--echo #

# Currently, we cannot nest more than 100 levels of objects and arrays. Make
# some documents that are nested to that exact depth.
CREATE TABLE t(jarray JSON, jobject JSON, jmix JSON) ROW_FORMAT=DYNAMIC;
INSERT INTO t VALUES ('1', '1', '1');
let $depth=0;
while ($depth < 100)
{
  eval UPDATE t SET jarray  = JSON_ARRAY(jarray),
                    jobject = JSON_OBJECT('a', jobject),
                    jmix    = CASE WHEN MOD($depth, 2) = 0
                                   THEN JSON_ARRAY(jmix)
                                   ELSE JSON_OBJECT('a', jmix)
                                   END;
  inc $depth;
}
SELECT JSON_DEPTH(jarray), JSON_DEPTH(jobject), JSON_DEPTH(jmix) FROM t;

SELECT JSON_DEPTH(CAST(jarray AS CHAR)),
       JSON_DEPTH(CAST(jobject AS CHAR)),
       JSON_DEPTH(CAST(jmix AS CHAR)) FROM t;

SELECT JSON_VALID(jarray), JSON_VALID(jobject), JSON_VALID(jmix) FROM t;

SELECT JSON_VALID(CAST(jarray AS CHAR)),
       JSON_VALID(CAST(jobject AS CHAR)),
       JSON_VALID(CAST(jmix AS CHAR)) FROM t;

--error ER_JSON_DOCUMENT_TOO_DEEP
INSERT INTO t(jarray) SELECT JSON_ARRAY(jarray) FROM t;
--error ER_JSON_DOCUMENT_TOO_DEEP
INSERT INTO t(jobject) SELECT JSON_OBJECT('a', jobject) FROM t;
--error ER_JSON_DOCUMENT_TOO_DEEP
INSERT INTO t(jmix) SELECT JSON_ARRAY(jmix) FROM t;
--error ER_JSON_DOCUMENT_TOO_DEEP
INSERT INTO t(jmix) SELECT JSON_OBJECT('a', jmix) FROM t;

CREATE TABLE too_deep_docs(id INT PRIMARY KEY AUTO_INCREMENT, x text);
INSERT INTO too_deep_docs(x) SELECT CONCAT('[', jarray, ']') FROM t;
INSERT INTO too_deep_docs(x) SELECT CONCAT('[', jobject, ']') FROM t;
INSERT INTO too_deep_docs(x) SELECT CONCAT('[', jmix, ']') FROM t;
INSERT INTO too_deep_docs(x) SELECT CONCAT('{"a":', jarray, '}') FROM t;
INSERT INTO too_deep_docs(x) SELECT CONCAT('{"a":', jobject, '}') FROM t;
INSERT INTO too_deep_docs(x) SELECT CONCAT('{"a":', jmix, '}') FROM t;
INSERT INTO too_deep_docs(x) SELECT CONCAT('["abc", [', jarray, ']]') FROM t;
INSERT INTO too_deep_docs(x)
  SELECT CONCAT('{"a":2,"b":{"c":', jobject, '}}') FROM t;

let $count= `SELECT COUNT(*) FROM too_deep_docs`;
while ($count)
{
  --error ER_JSON_DOCUMENT_TOO_DEEP
  eval SELECT CAST(x AS JSON) FROM too_deep_docs WHERE id=$count;
  --error ER_JSON_DOCUMENT_TOO_DEEP
  eval SELECT JSON_DEPTH(x) FROM too_deep_docs WHERE id=$count;
  --error ER_JSON_DOCUMENT_TOO_DEEP
  eval SELECT JSON_VALID(x) FROM too_deep_docs WHERE id=$count;
  --error ER_JSON_DOCUMENT_TOO_DEEP
  eval INSERT INTO t(jarray) SELECT x FROM too_deep_docs WHERE id=$count;
  dec $count;
}

--error ER_JSON_DOCUMENT_TOO_DEEP
SELECT CAST(JSON_ARRAY(jarray) AS CHAR) FROM t;
--error ER_JSON_DOCUMENT_TOO_DEEP
SELECT CAST(JSON_OBJECT('a', jobject) AS CHAR) FROM t;
--error ER_JSON_DOCUMENT_TOO_DEEP
SELECT CAST(JSON_ARRAY(jmix) AS CHAR) FROM t;
--error ER_JSON_DOCUMENT_TOO_DEEP
SELECT CAST(JSON_OBJECT('a', jmix) AS CHAR) FROM t;

DROP TABLE t, too_deep_docs;

--echo #
--echo # Bug#21054252 QUERY HAVING SQL_BIG_RESULT ON JSON DATA GIVES INVALID
--echo #              DATA ERROR
--echo #
CREATE TABLE t1 (i1 INT, KEY(i1));
CREATE TABLE t2 (i2 INT, j2 JSON);
INSERT INTO t2 (i2, j2) VALUES
(1, '["a"]'),
(2, '["ab"]');
--source include/turn_off_only_full_group_by.inc
SELECT SQL_BIG_RESULT i1, j2
FROM t2 LEFT JOIN t1 ON i2 < i1 GROUP BY j2 ORDER BY i2;
--source include/restore_sql_mode_after_turn_off_only_full_group_by.inc
DROP TABLE t1, t2;

--echo #
--echo # Bug#21104470 WL8132:ASSERTION `! IS_SET()' FAILED.
--echo #

# abbreviated test case

CREATE TABLE t_21104470(j JSON);
INSERT INTO t_21104470 VALUES (NULL), (NULL);
# Should return one row with the value NULL.
SELECT j FROM t_21104470 GROUP BY j;
# This one should return NULL too
SELECT DISTINCT j FROM t_21104470;

DROP TABLE t_21104470;

CREATE TABLE t(j JSON NOT NULL);
# There is no default for the field, and it can't be NULL, so this should fail.
--error ER_NO_DEFAULT_FOR_FIELD
INSERT INTO t(j) VALUES (DEFAULT);
# Loosen up the checks. It works in non-strict mode.
SET sql_mode = '';
INSERT INTO t(j) VALUES (DEFAULT);
# This query failed with 'invalid data' before bug#21104470.
SELECT * FROM t;
SET sql_mode = default;
DROP TABLE t;

--echo #
--echo # Bug#21072360 ASSERTION `(*A)->FIELD_TYPE() == MYSQL_TYPE_JSON ||
--echo #              (*B)->FIELD_TYPE() == FAILED
--echo #

CREATE TABLE t (j JSON);
INSERT INTO t VALUES ('true'), ('"abc"');
SELECT j FROM t WHERE j <= 'xyz' AND j = 'abc';
DROP TABLE t;

--echo #
--echo # Bug#21094905 VALGRIND ERRORS WITH LATEST BUILDS OF WL7909
--echo #

CREATE TABLE F (i1 INT, j JSON);
INSERT INTO F VALUES (1, '1'), (2, '2');
CREATE TABLE H (i2 INT);
SELECT SUM(DISTINCT i2), j FROM F LEFT JOIN H ON i1 = i2 GROUP BY j ORDER BY j;
DROP TABLE F, H;

--echo #
--echo # Bug#21110783 WL8132:DEBUG CRASH AT WRAPPER_TO_STRING | SQL/JSON_DOM.CC
--echo #

CREATE TABLE t1 (i1 INT) ENGINE=MyISAM;
INSERT INTO t1 VALUES (1);
CREATE TABLE t2 (i2 INT, j JSON) ENGINE=MyISAM;
CREATE TABLE t3(v VARCHAR(100), j JSON) ENGINE=MyISAM;
INSERT INTO t3(v) SELECT j FROM t1 LEFT JOIN t2 ON i1 = i2 GROUP BY j;
INSERT INTO t3(j) SELECT j FROM t1 LEFT JOIN t2 ON i1 = i2 GROUP BY j;
SELECT * FROM t3;
DROP TABLE t1, t2, t3;

--echo #
--echo # Bug#21119971 WL8132:DEBUG CRASH AT ITEM_CACHE_JSON::CACHE_VALUE
--echo #
CREATE TABLE t1(j1 JSON);
CREATE TABLE t2(j2 JSON);
CREATE TABLE t3(j3 JSON);
INSERT INTO t1 VALUES ('1');
INSERT INTO t2 VALUES ('1');
INSERT INTO t3 VALUES ('1');
SELECT * FROM t1 WHERE j1 >= ALL (SELECT j3 FROM t2 LEFT JOIN t3 ON (j2 > j3));
DROP TABLE t1, t2, t3;

--echo #
--echo # Bug#21145759 ER_INVALID_CAST_TO_JSON ON CALL TO JSON_REMOVE
--echo #              WITH EMPTY ARG + ORDER BY
--echo #
CREATE TABLE t (pk INT PRIMARY KEY, col_json JSON);
INSERT INTO t VALUES (1, JSON_OBJECT());
SELECT JSON_REMOVE((SELECT col_json FROM t WHERE pk = 3),
                  '$.testpath1') AS field1
FROM t HAVING field1 >= 'c' ORDER BY field1;
SELECT JSON_REMOVE((SELECT col_json FROM t WHERE pk = 3),
                  '$.testpath1') AS field1
FROM t HAVING field1 >= 'c';
SELECT JSON_REMOVE((SELECT col_json FROM t WHERE pk = 3),
                  '$.testpath1') AS field1
FROM t ORDER BY field1;
SELECT JSON_REMOVE((SELECT col_json FROM t WHERE pk = 3),
                  '$.testpath1') AS field1
FROM t;
DROP TABLE t;

--echo #
--echo # Bug#21135312 BUG IN JSON COMPARE OPARATOR WHEN USING JSON_QUOTE
--echo #
CREATE TABLE t1(c VARCHAR(10) CHARACTER SET latin1);
INSERT INTO t1 VALUES ('abc'), ('"abc"');
CREATE TABLE t2(j JSON);
INSERT INTO t2 VALUES ('"abc"');
SELECT c,
       c = CAST('"abc"' AS JSON) AS eq1,
       c = JSON_EXTRACT('"abc"', '$') AS eq2,
       c = j AS eq3
FROM t1, t2 ORDER BY c;
DROP TABLE t1, t2;

--echo #
--echo # Bug#21147943 JSON_CONTAINS() RETURNS INCORRECT RESULT
--echo #
SELECT JSON_CONTAINS('[1, 2, [4] ]', '{ "b" : 2 }');
SELECT JSON_CONTAINS('[1, 2, [4,5] ]', '[1,2,3,4,5,6,7,8,9]');
SELECT JSON_CONTAINS('[1, 2, [4,5] ]', '[111111111111111111]');

--echo #
--echo # Bug#21169109 WL8249:WRONG RESULTS WHILE COMPARING A JSON COLUMN
--echo #              WITH AN INDEXED INT COLUMN
--echo #

CREATE TABLE t1(j JSON, i INT);
INSERT INTO t1 VALUES
('0', 0), ('1', 1), ('1.0', 2), ('[1,2,3]', 3), ('{}', 4), ('"abc"', 5);
ANALYZE TABLE t1;
let $query1=SELECT * FROM t1 WHERE j > i ORDER BY i;
let $query2=SELECT * FROM t1 AS a, t1 AS b WHERE a.j > b.i ORDER BY b.i, a.i;
eval EXPLAIN $query1;
eval EXPLAIN $query2;
eval $query1;
eval $query2;
--echo # Create an index on the INT column and verify that it is not used.
CREATE INDEX t1_idx ON t1(i);
ANALYZE TABLE t1;
eval EXPLAIN $query1;
eval EXPLAIN $query2;
eval $query1;
eval $query2;
CREATE TABLE t2(i INT, vc VARCHAR(10))
ENGINE=InnoDB CHARACTER SET 'utf8mb4' COLLATE 'utf8mb4_bin';
INSERT INTO t2 VALUES (1, 'abc'), (2, '"abc"');
ANALYZE TABLE t2;
let $query= SELECT i FROM t2 WHERE vc = CAST('"abc"' AS JSON);
eval EXPLAIN $query;
eval $query;
--echo # Create an index on the VARCHAR column and verify that it is not used.
--echo # Used to return 2 instead of 1.
CREATE INDEX t2_idx ON t2(vc);
ANALYZE TABLE t2;
eval EXPLAIN $query;
eval $query;
DROP INDEX t2_idx ON t2;
--echo # Create a unique index on the VARCHAR column and verify that it is not
--echo # used. Used to return an empty result.
CREATE UNIQUE INDEX t2_idx ON t2(vc);
ANALYZE TABLE t2;
eval EXPLAIN $query;
eval $query;
DROP TABLE t1, t2;

# test that expanded escape sequences are not truncated
create table t_varchar( a varchar(3) );
insert into t_varchar values ( json_unquote( '"\\u0000\\u0001\\u0002"' ) );
select length(a) l from t_varchar;
select length( json_quote( a ) ) l, json_quote( a ) v from t_varchar;
select * from
(
 select length( json_quote( a ) ) as field0,
 json_quote( a ) as field1
 from t_varchar
) as derived_table;
drop table t_varchar;

--echo #
--echo # Bug#21193273 CREATE TABLE SELECT JSN_QUOTE() RESULTS
--echo # IN TRUNCATED DATA
--echo #

set names latin1;

SELECT JSON_QUOTE('table') AS field1;
CREATE TABLE t SELECT JSON_QUOTE('table') AS field1;
SELECT * FROM t;
DESCRIBE t;
DROP TABLE t;

SELECT JSON_UNQUOTE( '"table"' ) AS field1;
CREATE TABLE t1 SELECT JSON_unquote( '"table"' ) AS field1;
SELECT * FROM t1;
DESCRIBE t1;
DROP TABLE t1;

set names default;

--echo #
--echo # Bug#25455065 JSON_QUOTE AND JSON_UNQUOTE
--echo #              ARE WRONG FOR MULTIBYTE CHARSETS
--echo #

set names utf8mb4;

SELECT JSON_QUOTE('table') AS field1;
CREATE TABLE t SELECT JSON_QUOTE('table') AS field1;
SELECT * FROM t;
DESCRIBE t;
DROP TABLE t;

SELECT JSON_UNQUOTE( '"table"' ) AS field1;
CREATE TABLE t1 SELECT JSON_unquote( '"table"' ) AS field1;
SELECT * FROM t1;
DESCRIBE t1;
DROP TABLE t1;

set names default;

--echo #
--echo # Bug#21230644 JSON_MERGE MEMORY USAGE
--echo #

CREATE TABLE t (doc json);
INSERT INTO t VALUES('{"array":[1,2,3,4]}');
UPDATE t SET doc=JSON_MERGE_PRESERVE(doc, doc);
UPDATE t SET doc=JSON_MERGE_PRESERVE(doc, doc);
drop table t;

--echo #
--echo # Bug#21224267 DEEPLY NESTED JSON OBJECTS MAY CAUSE CRASH
--echo #
CREATE TABLE t (j JSON);
--error ER_JSON_DOCUMENT_TOO_DEEP
INSERT INTO t VALUES (CONCAT(REPEAT('{"v":', 5000), '1', REPEAT('}', 5000)));
--error ER_JSON_DOCUMENT_TOO_DEEP
INSERT INTO t VALUES (CONCAT(REPEAT('{"v":[', 5000), '1', REPEAT(']}', 5000)));
--error ER_JSON_DOCUMENT_TOO_DEEP
INSERT INTO t VALUES (CONCAT(REPEAT('[{"v":', 5000), '1', REPEAT('}]', 5000)));
DROP TABLE t;

--echo #
--echo # JSON should work with INSERT .. ON DUPLICATE KEY UPDATE
--echo #
CREATE TABLE t(id INT PRIMARY KEY, j JSON);
INSERT INTO t VALUES (1, '[1]')
ON DUPLICATE KEY UPDATE j = JSON_OBJECT("a", VALUES(j));
SELECT * FROM t;
INSERT INTO t VALUES (1, '[1,2]')
ON DUPLICATE KEY UPDATE j = JSON_OBJECT("ab", VALUES(j));
SELECT * FROM t;
INSERT INTO t VALUES (1, '[1,2,3]')
ON DUPLICATE KEY UPDATE j = JSON_OBJECT("abc", VALUES(j));
SELECT * FROM t;
DROP TABLE t;

--echo #
--echo # Bug#21278178 JSON_QUOTE(LITERAL) GETS APPLIED DOUBLE DURING SELECT
--echo #              FROM TABLE WITH MTR
--echo #
CREATE TABLE t(x INT);
INSERT INTO t VALUES (1), (2), (3);
SET NAMES latin1;
SELECT JSON_QUOTE('abc') FROM t;
SET NAMES utf8mb4;
SELECT JSON_QUOTE('abc') FROM t;
SET NAMES default;
DROP TABLE t;

--echo #
--echo # Bug#21291993 ASSERT `0' FAILED AT THD::SEND_STATEMENT_STATUS()
--echo #              ON JSON_SEARCH(NULL, ...)+JOIN
--echo #
CREATE TABLE t (pk INT NOT NULL PRIMARY KEY, col_varchar VARCHAR(1));
SELECT COUNT(*), JSON_SEARCH(NULL, 'one', '5%')
FROM t t1, t t2 WHERE t1.pk = t2.pk;
DROP TABLE t;

--echo #
--echo # Bug#21376088 JSON: CRASH IN VAL_JSON_FUNC_FIELD_SUBSELECT
--echo #
create table t(a json);
insert into t values('{}');
select a from t where (select a from t where 1) in (select 1 from t); # repro
select a from t where (select a from t where 1) in (select cast('{}' as json) from t);
drop table t;

--echo #
--echo # Bug#21437989: ASSERTION FAILED:
--echo #               JSON_BINARY::PARSE_BINARY(PTR, LENGTH).IS_VALID()
--echo #
CREATE TABLE t(j JSON NOT NULL);
--error ER_NO_DEFAULT_FOR_FIELD
INSERT INTO t VALUES ();
--error ER_BAD_NULL_ERROR
INSERT INTO t VALUES (NULL);
INSERT IGNORE INTO t VALUES ();
INSERT IGNORE INTO t VALUES (NULL);
SELECT * FROM t;
# The next two statements used to trigger an assertion.
INSERT INTO t SELECT j FROM t;
REPLACE INTO t SELECT j FROM t;
SELECT * FROM t;
DROP TABLE t;

--echo #
--echo # Bug#21448719: WRONG RESULT FOR JSON VALUE IN OUTER JOIN WITH VIEW
--echo #
CREATE TABLE t1(j1 JSON);
CREATE TABLE t2(j2 JSON);
CREATE VIEW v AS SELECT CAST('1' AS JSON) AS jv, j2 FROM t2;
INSERT INTO t1 VALUES ('1');
SELECT j1, jv, j2, JSON_ARRAY(j1, jv, j2) FROM t1 LEFT JOIN v ON j1 = jv;
INSERT INTO t2 VALUES ('1');
SELECT j1, jv, j2, JSON_ARRAY(j1, jv, j2) FROM t1 LEFT JOIN v ON j1 = jv;
DROP TABLE t1, t2;
DROP VIEW v;


--echo #
--echo # Bug#21472872 WRONG RESULTS CAUSED BY PATH LEG POPPING
--echo # IN JSON FUNCTIONS
--echo #

create table tdoc( id int, doc json );
insert into tdoc values
( 1, '[]' ),
( 2, '{ "a": { "b": true } }' );

select id, json_insert( doc, '$.a.c', false ) from tdoc where id = 2;
select id, json_insert( doc, '$.a.c', false ) from tdoc order by id;

drop table tdoc;
create table tdoc( id int, doc json, new_value varchar( 10 ) );
insert into tdoc values
( 1, '{ "a": { "b": true } }', null ),
( 2, '{ "a": { "b": true } }', 'abc' );

select id, json_insert( doc, '$.a.c', new_value ) from tdoc where id = 2;
select id, json_insert( doc, '$.a.c', new_value ) from tdoc order by id;

drop table tdoc;

--echo #
--echo # Bug#21487833: DBUG_ABORT() IN JSON_WRAPPER::MAKE_HASH_KEY
--echo #               WITH ORDERED JSON
--echo #
--source include/turn_off_strict_mode.inc
CREATE TABLE t (a BLOB, b JSON NOT NULL);
INSERT INTO t VALUES ('', NULL), ('', NULL);
UPDATE t SET a = 1 ORDER BY b;
SELECT COUNT(a) FROM t GROUP BY b;
SELECT DISTINCT B FROM t;
SELECT b FROM t UNION DISTINCT SELECT b FROM t;
SELECT * FROM t ORDER BY b;
DROP TABLE t;
--source include/restore_strict_mode.inc

--echo #
--echo # Bug#21541481: MEMORY LEAK OF ALLOCATIONS MADE IN
--echo #               VAL_JSON_FUNC_FIELD_SUBSELECT
--echo #
--echo # This comparison gave valgrind leakage before the fix
create table t(a json not null) engine=innodb;
insert into t values('{}');
select row(uuid(), a) < row(a, str_to_date(1,1)) from t;
drop table t;
--echo # Bug#21547877: UPDATE/INSERT JSON COLUMN CRASHES IF EXPRESSION
--echo #               REFERS TO SELF
--echo #
SET NAMES latin1;
CREATE TABLE t (j JSON);
INSERT INTO t VALUES ('{}');
--error ER_INVALID_JSON_TEXT
UPDATE t SET j='1', j='1111-11-11', j=('1' NOT BETWEEN j AND '1');
SELECT * FROM t;
DROP TABLE t;
SET NAMES DEFAULT;

--echo #
--echo # Bug#21602361: ASSERTION FAILED: (VECTOR_IDX == -1) || (VECTOR_IDX >= 0)
--echo #
create table t(b int, key(b));
insert into t values(1),(2);

# more than one row
--error ER_TOO_MANY_ROWS
select json_length('{}',@uninitialized_21602361)
from t group by b into @uninitialized_21602361;

# The user variable is assumed to be constant for SELECT ... INTO, even
# though INTO clause refers the same variable.
set @initialized_21602361 = '$';
--error ER_TOO_MANY_ROWS
select json_length('{}',@initialized_21602361)
from t group by b into @initialized_21602361;

# invalid path
set @error_value_21602361 = '$[';
--error ER_INVALID_JSON_PATH
select json_length('{}',@error_value_21602361)
from t group by b into @error_value_21602361;

# more than one row
set @valid_path_21602361 = '$';
--error ER_TOO_MANY_ROWS
select concat( '$[', json_length('{}',@valid_path_21602361), ']' )
from t group by b into @valid_path_21602361;

# used to trip the assertion
set @null_value_21602361 = null;
--error ER_TOO_MANY_ROWS
select json_length('{}',@null_value_21602361)
from t group by b into @null_value_21602361;

drop table t;

--echo #
--echo # Bug#21649073: JSON_TYPE RETURNS OPAQUE FOR SOME BINARY VALUES
--echo #
SELECT JSON_TYPE(CAST(CAST('abcd' AS BINARY) AS JSON));
CREATE TABLE t (bn BINARY(5), vb VARBINARY(5),
                tb TINYBLOB, mb MEDIUMBLOB, bb BLOB, lb LONGBLOB);
INSERT INTO t (bn) VALUES (x'cafe');
UPDATE t SET vb = bn, tb = bn, mb = bn, bb = bn, lb = bn;
SELECT JSON_TYPE(CAST(bn AS JSON)) AS bn, JSON_TYPE(CAST(vb AS JSON)) AS vb,
       JSON_TYPE(CAST(tb AS JSON)) AS tb, JSON_TYPE(CAST(mb AS JSON)) AS mb,
       JSON_TYPE(CAST(bb AS JSON)) AS bb, JSON_TYPE(CAST(lb AS JSON)) AS lb
FROM t;
DROP TABLE t;

--echo #
--echo # Basic tests for inlined JSON path
--echo #
CREATE TABLE t1(autopk int primary key auto_increment, f1 JSON);
INSERT INTO t1(f1) VALUES
  ('{"a":1}'),
  ('{"a":3}'),
  ('{"a":2}'),
  ('{"a":11, "b":3}'),
  ('{"a":33, "b":1}'),
  ('{"a":22,"b":2}');
ANALYZE TABLE t1;

SELECT f1->"$.a" FROM t1 order by autopk;
EXPLAIN SELECT f1->"$.a" FROM t1 order by autopk;

SELECT f1->"$.a" FROM t1 WHERE f1->"$.b" > 1 order by autopk;
EXPLAIN SELECT f1->"$.a" FROM t1 WHERE f1->"$.b" > 1 order by autopk;

SELECT f1->"$.a", f1->"$.b" FROM t1 ORDER BY autopk;
EXPLAIN SELECT f1->"$.a", f1->"$.b" FROM t1 ORDER BY autopk;

--sorted_result
SELECT MAX(f1->"$.a"), f1->"$.b" FROM t1 GROUP BY f1->"$.b";
EXPLAIN SELECT MAX(f1->"$.a"), f1->"$.b" FROM t1 GROUP BY f1->"$.b";

SELECT JSON_OBJECT("c",f1->"$.b") AS f2
  FROM t1 HAVING JSON_TYPE(f2->"$.c") <> 'NULL' ORDER BY autopk;
EXPLAIN
  SELECT JSON_OBJECT("c",f1->"$.b") AS f2 FROM t1 HAVING JSON_TYPE(f2->"$.c") <> 'NULL' order by autopk;

--echo Test unquoting operator
INSERT INTO t1(f1) VALUES
  ('{"t":"a"}'),
  ('{"t":"b"}'),
  ('{"t":"c"}');
--echo Returned values should be quoted
SELECT f1->"$.t" FROM t1 WHERE f1->"$.t" <> 'NULL' order by autopk;
--echo Returned values should be unquoted
SELECT f1->>"$.t" FROM t1 WHERE f1->>"$.t" <> 'NULL' order by autopk;
EXPLAIN SELECT f1->>"$.t" FROM t1 WHERE f1->>"$.t" <> 'NULL';

--error ER_PARSE_ERROR
SELECT f1->>NULL FROM t1;
--error ER_INVALID_JSON_PATH
SELECT f1 ->> "NULL" FROM t1;
--error ER_INVALID_JSON_PATH
SELECT f1->>"NULL" FROM t1;
--error ER_INVALID_JSON_PATH
SELECT f1->>"!@#" FROM t1;
--error ER_PARSE_ERROR
SELECT COUNT(*)->>"$.t" FROM t1;

INSERT INTO t1(f1) VALUES
  ('[ { "a": 1 }, { "a": 2 } ]'),
  ('{ "a" : "foo", "b" : [ true, { "c" : 123, "c" : 456 } ] }'),
  ('{ "a" : "foo", "b" : [ true, { "c" : "123" } ] }'),
  ('{ "a" : "foo", "b" : [ true, { "c" : 123 } ] }');

SELECT
  f1->>"$**.b",
  cast(json_unquote(json_extract(f1,"$**.b")) as char),
  cast(f1->>"$**.b" as char) <=> cast(json_unquote(json_extract(f1,"$**.b")) as char)
  FROM t1 order by autopk;
SELECT
  f1->>"$.c",
  cast(json_unquote(json_extract(f1,"$.c")) as char),
  cast(f1->>"$.c"  as char) <=> cast(json_unquote(json_extract(f1,"$.c")) as char)
  FROM t1 order by autopk;
SELECT
  f1->>'$.b[1].c',
  cast(json_unquote(json_extract(f1,'$.b[1].c')) as char),
  cast(f1->>'$.b[1].c' as
  char)<=>cast(json_unquote(json_extract(f1,'$.b[1].c')) as char)
  FROM t1 order by autopk;
SELECT
  f1->'$.b[1].c',
  cast(json_extract(f1,'$.b[1].c') as char),
  cast(f1->'$.b[1].c' as char)
  <=>cast(json_extract(f1,'$.b[1].c') as char)
  FROM t1 order by autopk;
SELECT
  f1->>'$.b[1]',
  cast(json_unquote(json_extract(f1,'$.b[1]')) as char),
  cast(f1->>'$.b[1]' as char) <=>
  cast(json_unquote(json_extract(f1,'$.b[1]')) as char)
  FROM t1 order by autopk;
SELECT
  f1->>'$[0][0]',
  cast(json_unquote(json_extract(f1,'$[0][0]')) as char),
  cast(f1->>'$[0][0]' as char) <=>
  cast(json_unquote(json_extract(f1,'$[0][0]')) as char)
  FROM t1 order by autopk;
SELECT
  f1->>'$**[0]',
  cast(json_unquote(json_extract(f1,'$**[0]')) as char),
  cast(f1->>'$**[0]' as char) <=>
  cast(json_unquote(json_extract(f1,'$**[0]')) as char)
  FROM t1 order by autopk;
SELECT
  f1->> '$.a[0]',
  cast(json_unquote(json_extract(f1, '$.a[0]')) as char),
  cast(f1->> '$.a[0]' as char) <=> cast(json_unquote(json_extract(f1,
  '$.a[0]')) as char)
  FROM t1 order by autopk;
SELECT
  f1->>'$[0].a[0]',
  cast(json_unquote(json_extract(f1,'$[0].a[0]')) as char),
  cast(f1->>'$[0].a[0]' as char) <=>
  cast(json_unquote(json_extract(f1,'$[0].a[0]')) as char)
  FROM t1 order by autopk;
SELECT
  f1->>'$**.a',
  cast(json_unquote(json_extract(f1,'$**.a')) as char),
  cast(f1->>'$**.a' as char) <=> cast(json_unquote(json_extract(f1,'$**.a')) as char)
  FROM t1 order by autopk;
SELECT
  f1->>'$[0][0][0].a',
  cast(json_unquote(json_extract(f1,'$[0][0][0].a')) as char),
  cast(f1->>'$[0][0][0].a' as char) <=>
  cast(json_unquote(json_extract(f1,'$[0][0][0].a')) as char)
  FROM t1 order by autopk;
SELECT
  f1->>'$[*].b',
  cast(json_unquote(json_extract(f1,'$[*].b')) as char),
  cast(f1->>'$[*].b' as char) <=>
  cast(json_unquote(json_extract(f1,'$[*].b')) as char)
  FROM t1 order by autopk;
SELECT
  f1->>'$[*].a',
  cast(json_unquote(json_extract(f1,'$[*].a')) as char),
  cast(f1->>'$[*].a' as char) <=>
  cast(json_unquote(json_extract(f1,'$[*].a')) as char)
  FROM t1 order by autopk;

DROP TABLE t1;

--echo #
--echo # Bug#21072360 ASSERTION `(*A)->FIELD_TYPE() == MYSQL_TYPE_JSON ||
--echo #              (*B)->FIELD_TYPE() == FAILED
--echo #

--echo #
--echo # Bug#21144949 WL8132:DELETE QUERY HAVING A SUB-QUERY DOES NOT
--echo #              DELETE ROWS IN THE TABLE
--echo #
CREATE TABLE t(j JSON);
INSERT INTO t VALUES (JSON_OBJECT('a', 'b')), (JSON_OBJECT('a', 'b'));
DELETE FROM t WHERE j IN (SELECT JSON_OBJECT('a', 'b') FROM DUAL WHERE 1);
SELECT * FROM t;
INSERT INTO t VALUES (JSON_OBJECT('a', 'b')), (JSON_OBJECT('a', 'b'));
DELETE FROM t WHERE j IN (SELECT JSON_OBJECT('a', 'b') FROM DUAL);
SELECT * FROM t;
INSERT INTO t VALUES (JSON_OBJECT('a', 'b')), (JSON_OBJECT('a', 'b'));
DELETE FROM t WHERE j IN (SELECT CAST(NULL AS JSON) FROM DUAL);
DELETE FROM t WHERE j IN (SELECT CAST(NULL AS JSON) FROM DUAL WHERE 1);
--error ER_INVALID_JSON_TEXT_IN_PARAM
DELETE FROM t WHERE j IN (SELECT CAST('not json' AS JSON) FROM DUAL);
--error ER_INVALID_JSON_TEXT_IN_PARAM
DELETE FROM t WHERE j IN (SELECT CAST('not json' AS JSON) FROM DUAL WHERE 1);
SELECT * FROM t;
DROP TABLE t;

--echo #
--echo # Bug#22366102: EMPTY STRINGS AS KEYS PROBABLY NOT HANDLED CORRECTLY
--echo #               BY JSON_EXTRACT()
--echo #
CREATE TABLE t(j JSON);
INSERT INTO t VALUES ('{"":"a"}'), ('{"":{"":"a"}}'), ('{"a":{"":{"b":"a"}}}');
--sorted_result
SELECT JSON_UNQUOTE(JSON_SEARCH(j, 'one', 'a')) AS path FROM t;
--sorted_result
SELECT j->'$.""' AS c1, j->'$."".""' AS c2, j->'$.a."".b' AS c3 FROM t;
DROP TABLE t;
--error ER_INVALID_JSON_PATH
SELECT JSON_EXTRACT('[1]', '$.');
--error ER_INVALID_JSON_PATH
SELECT JSON_EXTRACT('[1]', '$..');
--error ER_INVALID_JSON_PATH
SELECT JSON_EXTRACT('[1]', '$.a..b');

--echo #
--echo # Bug#22477046: HANDLE_FATAL_SIGNAL (SIG=6) IN ITEM_FUNC_SP::VAL_JSON
--echo #
CREATE TABLE t1(j JSON);
CREATE TABLE t2(vc VARCHAR(10));
CREATE FUNCTION f1(i INT) RETURNS INT RETURN i;
--error ER_INVALID_JSON_TEXT
INSERT INTO t1 SELECT f1(3);
INSERT INTO t2 SELECT f1(3);
CREATE FUNCTION f2(vc VARCHAR(10)) RETURNS VARCHAR(10) RETURN vc;
INSERT INTO t1 SELECT f2('[1,2,3]');
INSERT INTO t2 SELECT f2('[1,2,3]');
CREATE FUNCTION f3(j JSON) RETURNS JSON RETURN j;
INSERT INTO t1 SELECT f3('[4,5,6]');
INSERT INTO t2 SELECT f3('[4,5,6]');
--sorted_result
SELECT * FROM t1;
--sorted_result
SELECT * FROM t2;
DROP TABLE t1, t2;
DROP FUNCTION f1;
DROP FUNCTION f2;
DROP FUNCTION f3;

--echo # Bug#22522073: Assertion failed: !thd->is_error() in optimize_cond()

CREATE TABLE s(a INTEGER, b CHAR(1));
INSERT INTO s VALUES (1,0), (1,0), (1,0);

CREATE TABLE t(c JSON);
INSERT INTO t VALUES (), (), ();

--error ER_SUBQUERY_NO_1_ROW
SELECT 1
FROM s WHERE a NOT IN (SELECT 1
                       FROM s WHERE (SELECT c FROM t) = b);

DROP TABLE s, t;

--echo #
--echo # Bug#22546013: REMOVE DEPENDENCY ON BOOST IN ITEM_JSON_FUNC.CC
--echo #
# This test case does not show a bug, it just exercises a code path that was
# changed by the fix. The point of the test case is to verify that memory is
# freed when a Json_scalar_holder is reused over multiple rows. The test case
# must use a scalar type that actually allocates some memory on the heap.
# Json_string does that, in its std::string member. Additionally, the string
# must be relatively long, so that std::string cannot avoid heap allocation with
# Small String Optimization. And the test must run under Valgrind or a similar
# tool to detect memory leaks.
CREATE TABLE t(j JSON, vc VARCHAR(99));
INSERT INTO t VALUES ('"abc"', REPEAT('abc', 33)), ('"abc"', 'abc');
INSERT INTO t SELECT * FROM t;
SELECT * FROM t WHERE j = vc;
DROP TABLE t;

--echo #
--echo # Bug#22816576: ASSERTION FAILED: !THD->IS_ERROR() IN OPTIMIZE_COND()
--echo #
CREATE TABLE t(a DATE);
--error ER_INVALID_JSON_PATH
SELECT 1 FROM t WHERE 1 <>
(
 SELECT 1 FROM
 (
  SELECT 1 FROM t e
  JOIN t ON t.a <=> JSON_REPLACE(JSON_OBJECT('', ''), '', '')
  WHERE e.a
 ) w
);
DROP TABLE t;

--echo #
--echo # Bug#23209914: ASSERTION FAILED: THIS_TYPE != ENUM_JSON_TYPE::J_ERROR
--echo #
CREATE PROCEDURE p(a JSON) SELECT 1 WHERE a = 1;
CALL p(NULL);
DROP PROCEDURE p;

#
# Bug#21573154: ASSERTION `(KP1 + KP1_LEN) < (S1 + M_SIZE)
#
CREATE TABLE t1 (pk INT AUTO_INCREMENT KEY, f1 INT, fj JSON);
INSERT INTO t1 VALUES(1, NULL, JSON_OBJECT('aaa','bbb'));
INSERT INTO t1 VALUES(2, NULL, JSON_OBJECT('bbb','aaa'));
SELECT * FROM t1 ORDER BY f1, pk, fj;

DROP TABLE t1;

--echo #
--echo # Bug#25461627: VALGRIND WARNING WHEN UPDATING A JSON COLUMN
--echo #
CREATE TABLE t (j JSON);
INSERT INTO t VALUES (NULL);
UPDATE t SET j = JSON_ARRAY(REPEAT('a', 32)),
             j = JSON_EXTRACT(j, '$[0]');
SELECT * FROM t;
UPDATE t SET j = JSON_ARRAY(CAST(REPEAT('b', 32) AS BINARY)),
             j = JSON_EXTRACT(j, '$[0]');
SELECT * FROM t;
UPDATE t SET j = JSON_ARRAY(JSON_ARRAY(REPEAT('c', 32))),
             j = JSON_EXTRACT(j, '$[0]');
SELECT * FROM t;
UPDATE t SET j = JSON_ARRAY(JSON_OBJECT(REPEAT('d', 16), REPEAT('e', 16))),
             j = JSON_EXTRACT(j, '$[0]');
SELECT * FROM t;
DROP TABLE t;

--echo #
--echo # WL#9191: JSON_PRETTY function
--echo #
CREATE TABLE t(id INT PRIMARY KEY AUTO_INCREMENT,
               uc VARCHAR(128) CHARACTER SET utf8mb4,
               lc VARCHAR(128) CHARACTER SET latin1,
               j JSON);
INSERT INTO t(uc) VALUES
(NULL),
('{}'),
('[]'),
('[1,2,3]'),
('{"a":1,"b":2}'),
('{"a":[1,{}],"b":[]}'),
('{"a":{"b":{"c":{"d":{"e":{"f":null}}}}}}'),
('true'),
('false'),
('null'),
('1'),
('1.1'),
('"hello"'),
(JSON_ARRAY('abc
def', '"abc"'));
UPDATE t SET lc = uc, j = uc;
SELECT JSON_PRETTY(uc) AS c1 FROM t ORDER BY id;
SELECT JSON_PRETTY(lc) AS c1 FROM t ORDER BY id;
SELECT JSON_PRETTY(j) AS c1 FROM t ORDER BY id;
SELECT DISTINCT CHARSET(JSON_PRETTY(uc)) AS c1,
                CHARSET(JSON_PRETTY(lc)) AS c2,
                CHARSET(JSON_PRETTY(j)) AS c3 FROM t;
DROP TABLE t;
--error ER_INVALID_JSON_TEXT_IN_PARAM
SELECT JSON_PRETTY('this is not JSON') AS j;
--error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
SELECT JSON_PRETTY();
--error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
SELECT JSON_PRETTY('{}', 2);

--echo #
--echo # WL#9192: Add JSON_STORAGE_SIZE / JSON_STORAGE_FREE functions
--echo #
--error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
SELECT JSON_STORAGE_SIZE();
--error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
SELECT JSON_STORAGE_FREE();
--error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
SELECT JSON_STORAGE_SIZE('{}', '[]');
--error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
SELECT JSON_STORAGE_FREE('{}', '[]');
--error ER_INVALID_JSON_TEXT_IN_PARAM
SELECT JSON_STORAGE_SIZE('this is not JSON');
--error ER_INVALID_JSON_TEXT_IN_PARAM
SELECT JSON_STORAGE_FREE('this is not JSON');
SELECT JSON_STORAGE_SIZE(NULL);
SELECT JSON_STORAGE_FREE(NULL);
SELECT JSON_STORAGE_SIZE(JSON_ARRAY(1,2,3));
SELECT JSON_STORAGE_FREE(JSON_ARRAY(1,2,3));
CREATE TABLE t(id INT PRIMARY KEY, j JSON, v VARCHAR(100));
INSERT INTO t(id, j) VALUES (0, NULL), (1, '[["abc", "def", "ghi"], "jkl"]');
UPDATE t SET v = j;
let $dump_table=
SELECT *,
       JSON_STORAGE_SIZE(j), JSON_STORAGE_SIZE(v),
       JSON_STORAGE_SIZE(j->'\$[0]'), JSON_STORAGE_SIZE(v->'\$[0]'),
       JSON_STORAGE_FREE(j), JSON_STORAGE_FREE(v),
       JSON_STORAGE_FREE(j->'\$[0]'), JSON_STORAGE_FREE(v->'\$[0]')
       FROM t ORDER BY id;
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0][1]', NULL, '$[1]', 'X'),
             v = JSON_SET(v, '$[0][1]', NULL, '$[1]', 'X');
--eval $dump_table
DROP TABLE t;

#
# Get some extra coverage for partial update of JSON, WL#8963.
#

CREATE TABLE t(id INT PRIMARY KEY, j JSON);
INSERT INTO t VALUES
(1, '{"a":"a"}'), (2, '{"a":"b", "b":"b"}'), (3, '{"a":"c", "c":"c"}');
let $dump_table= SELECT *, JSON_STORAGE_SIZE(j) ss,
                        JSON_STORAGE_FREE(j) sf FROM t ORDER BY id;
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$.a', 'x');
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$.a', 'y', '$.b', j->'$.a');
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$.a', 'z'), j = JSON_SET(j, '$.b', j->'$.a');
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$.a', 'w'),
             j = JSON_SET(j, '$.a', 'v', '$.b', j->'$.a');
--eval $dump_table
UPDATE t SET j = JSON_REPLACE(j, '$.a', '1', '$.c', '2', '$.a', '3');
--eval $dump_table
UPDATE t SET j = JSON_REPLACE(j, '$.a', '4'), j = JSON_REPLACE(j, '$.c', '5'),
             j = JSON_REPLACE(j, '$.a', '6');
--eval $dump_table
UPDATE t SET j = JSON_REPLACE(j, '$.a', '');
--eval $dump_table
UPDATE t SET j = JSON_REPLACE(j, '$.a', 'a');
--eval $dump_table
UPDATE t SET j = JSON_REPLACE(j, '$.a', 'ab');
--eval $dump_table
UPDATE t SET j = JSON_REPLACE(JSON_REPLACE(j, '$.a', 'A'), '$.b', true);
--eval $dump_table
UPDATE t SET j = JSON_REPLACE(JSON_REPLACE(j, '$.a', 'X'), '$.unknown', 'Y');
--eval $dump_table
UPDATE t SET j = JSON_REPLACE(JSON_REPLACE(j, '$.unknown', 'Z'), '$.a', 'W');
--eval $dump_table
UPDATE t SET j = JSON_REPLACE(JSON_REPLACE(j, '$.unknown1', 1),
                              '$.unknown2', 2);
--eval $dump_table
UPDATE t SET j = JSON_SET(JSON_REPLACE(j, '$.a', 99), '$.b', 88);
--eval $dump_table
UPDATE t SET j = JSON_REPLACE(JSON_SET(j, '$.a', 77), '$.b', 66);
--eval $dump_table
UPDATE t SET j = JSON_REPLACE(JSON_SET(j, '$.c', 0), '$.a', 1);
--eval $dump_table
UPDATE t SET j = JSON_SET(JSON_REPLACE(JSON_SET(j, '$.a', 11),
                                       '$.unknown', 12),
                          '$.b', 13);
--eval $dump_table
DROP TABLE t;

CREATE TABLE t(id INT PRIMARY KEY, j1 JSON, j2 JSON);
INSERT INTO t VALUES
(1, '{"a":"a","b":"b"}', '{"a":"a","b":"b"}'),
(2, NULL, NULL),
(3, '{"a":"aa","b":"bb"}', '{"a":"aa","b":"bb"}');
ANALYZE TABLE t;
let $dump_table= SELECT *, JSON_STORAGE_SIZE(j1) ss1,
                           JSON_STORAGE_FREE(j1) sf1,
                           JSON_STORAGE_SIZE(j2) ss2,
                           JSON_STORAGE_FREE(j2) sf2 FROM t ORDER BY id;
--eval $dump_table
let $query= UPDATE t SET j1 = JSON_SET(j1, '\$.a', 'x'),
                         j2 = JSON_SET(j2, '\$.a', 'y'),
                         j1 = JSON_SET(j1, '\$.b', 'z');
--eval EXPLAIN FORMAT=JSON $query
SET optimizer_trace="enabled=on";
--eval $query
SET optimizer_trace="enabled=off";
SELECT trace FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
--eval $dump_table
let $query= UPDATE t SET j1 = JSON_SET(j1, '\$.a', 's'),
                         j2 = JSON_SET(j2, '\$.a', 'abcdefghi'),
                         j1 = JSON_SET(j1, '\$.b', 't');
--eval EXPLAIN FORMAT=JSON $query
SET optimizer_trace="enabled=on";
--eval $query
SET optimizer_trace="enabled=off";
SELECT trace FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
--eval $dump_table
--let $query= UPDATE t SET j1 = JSON_SET(j1, '\$.x.y.z', 'abc', '\$.b', 'X')
--eval EXPLAIN FORMAT=JSON $query
SET optimizer_trace="enabled=on";
--eval $query
SET optimizer_trace="enabled=off";
SELECT trace FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
--eval $dump_table
--let $query= UPDATE t SET j1 = JSON_REPLACE(j1, '\$.x', 'abc', '\$.b', 'Y')
--eval EXPLAIN FORMAT=JSON $query
SET optimizer_trace="enabled=on";
--eval $query
SET optimizer_trace="enabled=off";
SELECT trace FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
--eval $dump_table
--let $query= UPDATE t SET j1 = JSON_SET(j1, '\$.x', 'abc', '\$.b', 'Z')
--eval EXPLAIN FORMAT=JSON $query
SET optimizer_trace="enabled=on";
--eval $query
SET optimizer_trace="enabled=off";
SELECT trace FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
--eval $dump_table
# No partial update here since input column is different from target column.
--let $query= UPDATE t SET j1 = JSON_SET(j2, '\$.a', 'ABC')
--eval EXPLAIN FORMAT=JSON $query
SET optimizer_trace="enabled=on";
--eval $query
SET optimizer_trace="enabled=off";
SELECT trace FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
--eval $dump_table
# No partial update here since JSON_ARRAY doesn't support partial update.
--let $query= UPDATE t SET j1 = JSON_ARRAY(j1, j1)
--eval EXPLAIN FORMAT=JSON $query
SET optimizer_trace="enabled=on";
--eval $query
SET optimizer_trace="enabled=off";
SELECT trace FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
--eval $dump_table
# No partial update of j1, but j2 should be partially updated.
let $query= UPDATE t SET j1 = JSON_SET(j2, '\$.a', 1),
                         j2 = JSON_SET(j2, '\$.a', 1);
--eval EXPLAIN FORMAT=JSON $query
SET optimizer_trace="enabled=on";
--eval $query
SET optimizer_trace="enabled=off";
SELECT trace FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
--eval $dump_table
DROP TABLE t;

CREATE TABLE t(j JSON,
               i INT DEFAULT 42,
               db DOUBLE DEFAULT 2.34e-10,
               dc DECIMAL(5, 3) DEFAULT 98.765,
               ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
               gc JSON GENERATED ALWAYS AS (JSON_ARRAY(i, db, dc, ts)));
INSERT INTO t(j, ts) VALUES
('[null, "abc", true, "def", false]', '2017-01-02 14:15:16');
let $dump_table= SELECT j, JSON_TYPE(j) jt, JSON_STORAGE_SIZE(j) ss,
                        JSON_STORAGE_FREE(j) sf FROM t;
--eval $dump_table
# The update of $[3] to a longer value should be able to reuse the space freed
# by the update of $[1] to a shorter value.
UPDATE t SET j = JSON_SET(j, '$[1]', 'xy', '$[3]', 'xyzw');
--eval $dump_table
# Again, increasing the size of $[3] is OK as long as $[1] shrinks first, also
# when done in two separate operations.
UPDATE t SET j = JSON_SET(j, '$[1]', 'a');
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[3]', 'abcde');
--eval $dump_table

# See what happens if one partial update overwrites a nested array that had a
# partial update performed to one of its elements previously in the same
# statement.
UPDATE t SET j = '[[[[[1]]]]]';
--eval $dump_table
UPDATE t SET j = JSON_SET(JSON_SET(j, '$[0][0][0]', 'ab'), '$[0][0]', 'cdefgh');
--eval $dump_table

# Replacing the root of a document will result in a full update.
UPDATE t SET j = JSON_REPLACE(j, '$', JSON_ARRAY(123));
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$', 456);
--eval $dump_table

# Replacing with non-existent paths (array access on object, or member access on
# array, or member/array access on scalar) cannot be performed as partial
# update.
UPDATE t SET j = JSON_ARRAY(1,2,3);
UPDATE t SET j = JSON_REPLACE(j, '$.a', 4);
--eval $dump_table
UPDATE t SET j = JSON_REPLACE(j, '$[1000]', 1000);
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[1000]', 1000);
--eval $dump_table
UPDATE t SET j = JSON_OBJECT('a',1,'b',2,'c',3);
UPDATE t SET j = JSON_REPLACE(j, '$[1]', 4);
--eval $dump_table
UPDATE t SET j = JSON_OBJECT('a',1,'b',2,'c',3);
UPDATE t SET j = JSON_REPLACE(j, '$[1]', 4);
--eval $dump_table
UPDATE t SET j = JSON_REPLACE(j, '$.a.b', 5);
--eval $dump_table
UPDATE t SET j = JSON_REPLACE(j, '$.a[2]', 6);
--eval $dump_table

# Test update to some other data types.
UPDATE t SET j = JSON_ARRAY('abcdefghijklmnopqrstuvwxyz');
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', 3.14e0);
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', CAST(10000000000 AS UNSIGNED));
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', CAST(4000000000 AS SIGNED));
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', CAST(123 AS UNSIGNED));
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', CAST(456 AS SIGNED));
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', -123);
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', -4000000000);
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', CAST(76543 AS SIGNED));
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', CAST(87654 AS UNSIGNED));
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', -100000);
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', TRUE);
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', NULL);
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', CAST('abc' AS BINARY));
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', CAST('2016-12-23 13:16:40' AS DATETIME));
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', CAST('2016-12-24' AS DATE));
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', CAST('14:17:41' AS TIME));
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', 1.23);
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', i);
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', db);
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', dc);
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', ts);
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', JSON_ARRAY());
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', JSON_ARRAY('abc'));
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', JSON_OBJECT());
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', JSON_OBJECT('abc', 'def'));
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', gc->'$[0]');
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', gc->'$[1]');
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', gc->'$[2]');
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', gc->'$[3]');
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[0]', gc->'$[4]');
--eval $dump_table

--echo # Test update with indexes relative to the end of the array.
UPDATE t SET j = JSON_ARRAY('a','b','c');
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[last]', 4);
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[last-1]', 5);
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[last-2]', 6);
--eval $dump_table
UPDATE t SET j = JSON_SET(j, '$[last-3]', 7);
--eval $dump_table
UPDATE t SET j = JSON_ARRAY('a','b');
--eval $dump_table
UPDATE t SET j = JSON_REPLACE(j, '$[last]', 3);
--eval $dump_table
UPDATE t SET j = JSON_REPLACE(j, '$[last-1]', 4);
--eval $dump_table
UPDATE t SET j = JSON_REPLACE(j, '$[last-2]', 5);
--eval $dump_table

DROP TABLE t;

# Some multi-table updates which perform partial updates.
CREATE TABLE t1(id INT PRIMARY KEY, j JSON, x INT);
INSERT INTO t1 VALUES (1, '{"a":"xy"}', 1), (2, '{"a":"zw"}', 2);
let $dump_t1= SELECT *, JSON_STORAGE_SIZE(j) ss,
                        JSON_STORAGE_FREE(j) sf FROM t1 ORDER BY id;
--eval $dump_t1
CREATE TABLE t2(id INT PRIMARY KEY, j JSON, x INT);
INSERT INTO t2 VALUES
(1, '{"b":"X"}', 3),
(2, '{"b":"Y"}', 4),
(3, '{"b":"Z"}', 9);
let $dump_t2= SELECT *, JSON_STORAGE_SIZE(j) ss,
                        JSON_STORAGE_FREE(j) sf FROM t2 ORDER BY id;
--eval $dump_t2
ANALYZE TABLE t1, t2;
let $query= UPDATE t1, t2
                   SET t1.j = JSON_SET(t1.j, '\$.a', t2.j->'\$.b')
                   WHERE t1.id = t2.id;
--replace_regex /"used_columns": \[[^]]*\]/"used_columns": "filtered due to bug#22472365"/
--eval EXPLAIN FORMAT=JSON $query
SET optimizer_trace="enabled=on";
--eval $query
SET optimizer_trace="enabled=off";
SELECT trace->'$**.json_partial_update'
       FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
--eval $dump_t1
--eval $dump_t2
let $query= UPDATE t1, t2 SET t1.j = JSON_SET(t1.j, '\$.a', t2.x),
                              t2.j = JSON_SET(t2.j, '\$.b', t1.x)
                          WHERE t1.id = t2.id;
--eval EXPLAIN FORMAT=JSON $query
SET optimizer_trace="enabled=on";
--eval $query
SET optimizer_trace="enabled=off";
SELECT trace->'$**.json_partial_update'
       FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
--eval $dump_t1
--eval $dump_t2
let $query= UPDATE t1, t2 SET t1.j = '[123]', t2.j = JSON_SET(t2.j, '\$.b', 99);
--replace_regex /"used_columns": \[[^]]*\]/"used_columns": "filtered due to bug#22472365"/
--eval EXPLAIN FORMAT=JSON $query
SET optimizer_trace="enabled=on";
--eval $query
SET optimizer_trace="enabled=off";
SELECT trace->'$**.json_partial_update'
       FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
--eval $dump_t1
--eval $dump_t2
DROP TABLE t1, t2;

# MyISAM doesn't support partial update, so don't expect partial
# update to be used.
CREATE TABLE t(j JSON) ENGINE=MyISAM;
INSERT INTO t VALUES ('["abcdef"]');
SELECT j, JSON_STORAGE_SIZE(j), JSON_STORAGE_FREE(j) FROM t;
SET optimizer_trace="enabled=on";
UPDATE t SET j = JSON_SET(j, '$[0]', 1);
SET optimizer_trace="enabled=off";
SELECT trace->'$**.json_partial_update'
       FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;
SELECT j, JSON_STORAGE_SIZE(j), JSON_STORAGE_FREE(j) FROM t;
DROP TABLE t;

--echo # Partial update on a DEFAULT NULL value in a NOT NULL column.
CREATE TABLE t(j JSON NOT NULL);
INSERT IGNORE INTO t VALUES ();
UPDATE t SET j = JSON_SET(j, '$[0]', 'abc');
SELECT j, JSON_STORAGE_SIZE(j), JSON_STORAGE_FREE(j) FROM t;
UPDATE IGNORE t SET j = DEFAULT;
UPDATE t SET j = JSON_REMOVE(j, '$[0]');
SELECT j, JSON_STORAGE_SIZE(j), JSON_STORAGE_FREE(j) FROM t;
DROP TABLE t;

--echo #
--echo # Bug#25893720: TABLE::SETUP_PARTIAL_UPDATE():
--echo #               ASSERTION `M_PARTIAL_UPDATE_INFO == NULLPTR' FAIL
--echo #
CREATE TEMPORARY TABLE t(j JSON);
INSERT INTO t VALUES ('["abc"]');
UPDATE t SET j = JSON_SET(j, '$[0]', 'de');
UPDATE t SET j = JSON_SET(j, '$[0]', 'f');
SELECT j, JSON_STORAGE_SIZE(j), JSON_STORAGE_FREE(j) FROM t;
DROP TABLE t;

--echo # Partial update in JSON_REMOVE.
CREATE TABLE t(id INT PRIMARY KEY AUTO_INCREMENT, j JSON);
INSERT INTO t(j) VALUES
(JSON_ARRAY('x', JSON_ARRAY(1, 2, 3, 'abc', 4, 5, 6))),
(JSON_OBJECT('x', JSON_OBJECT('a', 'one',
                              'b', 'two',
                              'c', 'ABC',
                              'defghijkl', 'm'),
             'y', 'z')),
('null'),
(NULL);
let $dump_table= SELECT *, JSON_STORAGE_SIZE(j) ss,
                        JSON_STORAGE_FREE(j) sf FROM t ORDER BY id;
--eval $dump_table
ANALYZE TABLE t;
--let $query= UPDATE t SET j = JSON_REMOVE(j, '\$.x.b', '\$[1][2]')
--eval EXPLAIN FORMAT=JSON $query
--eval $query
--eval $dump_table
--echo # See that the free space can be reused by increasing
--echo # the size of some values.
UPDATE t SET j = JSON_REPLACE(j, '$[1][2]', 'xyzw', '$.x.c', 'XYZW');
--eval $dump_table
--echo # Nested calls
UPDATE t SET j = JSON_REMOVE(JSON_REMOVE(j, '$[1][2]'), '$.x.c');
--eval $dump_table
UPDATE t SET j = JSON_REMOVE(JSON_SET(j, '$.x.a', 'xyz'), '$.x.a');
--eval $dump_table
UPDATE t SET j = JSON_REMOVE(JSON_SET(j, '$.x.a', 'zyx'), '$.x.a');
--eval $dump_table
UPDATE t SET j = JSON_SET(JSON_REMOVE(j, '$.x.a'), '$.x.a', 'abcd', '$.x.b', 9);
--eval $dump_table
UPDATE t SET j = JSON_SET(JSON_REMOVE(j, '$.x.b'), '$.x.a', 'efg');
--eval $dump_table
UPDATE t SET j = JSON_REMOVE(JSON_REMOVE(j, '$[0]', '$[0]', '$.x'),
                             '$.b', '$.y', '$.z');
--eval $dump_table
DROP TABLE t;

--echo #
--echo # WL#9692: Add JSON_MERGE_PATCH
--echo #

CREATE TABLE merge_t(
id INT PRIMARY KEY AUTO_INCREMENT,
target VARCHAR(100), patch VARCHAR(100)
);
INSERT INTO merge_t(target, patch) VALUES
('{"a":"b"}', '{"a":"c"}'),
('{"a":"b"}', '{"b":"c"}'),
('{"a":"b"}', '{"a":null}'),
('{"a":"b", "b":"c"}', '{"a":null}'),
('{"a":["b"]}', '{"a":"c"}'),
('{"a":"c"}', '{"a":["b"]}'),
('{"a": {"b":"c"}}', '{"a": {"b":"d", "c":null}}'),
('{"a":[{"b":"c"}]}', '{"a": [1]}'),
('["a","b"]', '["c","d"]'),
('{"a":"b"}', '["c"]'),
('{"a":"foo"}', 'null'),
('{"a":"foo"}', '"bar"'),
('{"e":null}', '{"a":1}'),
('[1,2]', '{"a":"b", "c":null}'),
('{}', '{"a":{"bb":{"ccc":null}}}'),
(NULL, '{}'),
('{}', NULL);
SELECT id, target, patch,
       JSON_MERGE_PATCH(target, patch) AS merged,
       JSON_EXTRACT(JSON_MERGE_PATCH(target, patch), '$.a') AS a
FROM merge_t ORDER BY id;
DROP TABLE merge_t;

SELECT JSON_MERGE_PATCH('{"a":"b"}', NULL, '{"c":"d"}');
SELECT JSON_MERGE_PATCH(NULL, '[1,2,3]');
SELECT JSON_MERGE_PATCH('{"a":"b"}', NULL, '[1,2,3]', '{"c":null,"d":"e"}');

--error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
SELECT JSON_MERGE_PATCH();
--error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
SELECT JSON_MERGE_PATCH('{}');
--error ER_INVALID_JSON_TEXT_IN_PARAM
SELECT JSON_MERGE_PATCH('{', '[1,2,3]');
--error ER_INVALID_JSON_TEXT_IN_PARAM
SELECT JSON_MERGE_PATCH('{"a":"b"}', '[1,');

# Local Variables:
# mode: sql
# sql-product: mysql
# comment-column: 48
# comment-start: "# "
# fill-column: 80
# End:
