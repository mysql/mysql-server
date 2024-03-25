# WL15426: Implement SHOW PARSE_TREE

--source include/have_debug.inc

SHOW PARSE_TREE SELECT t3.col3, t1.col1 / t2. col2 FROM tab t1
    JOIN tab t2 ON t1.col1=t2.col2 JOIN tab t3 ON t1.col1 <= t3.col3;
SHOW PARSE_TREE SELECT a, b, func(c), SUM(c) OVER (PARTITION BY a,b ) FROM Y;
SHOW PARSE_TREE SELECT a, b, c, SUM(c) OVER
  (PARTITION BY a,b
   OrDER              BY a,b RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW)
  FROM Y;
SHOW PARSE_TREE SELECT id, FIRST_VALUE(id) OVER w FROM t_date
  WINDOW w AS (ORDER BY id RANGE BETWEEN INTERVAL col1 DAY PRECEDING AND INTERVAL 1 DAY PRECEDING);

SHOW PARSE_TREE select (select 1) from t ORDER BY 1 DESC, 2;
SHOW PARSE_TREE SELECT 1 < 2, col1 NOT between 1 and 2, 1 IN (1, 2), 1 IN (1), 1 NOT IN (1);
SHOW PARSE_TREE SELECT avg(distinct col1) from tab GROUP BY col2, col3;
SHOW PARSE_TREE SELECT not 1 or 2 is true or 0 is not false alias_name;
SHOW PARSE_TREE select 1 and 2 or 3 and 4, {u '2022/22/2' }, 1 < all (select 1);
SHOW PARSE_TREE SELECT 1 - INTERVAL 2 HOUR_MINUTE, curtime(4), utc_time(2);
SHOW PARSE_TREE SELECT * FROM t1 WHERE
  MATCH(a,b) AGAINST ("collections" IN BOOLEAN MODE ) OR
  MATCH(a,b) AGAINST ("indexes" IN NATURAL LANGUAGE MODE WITH QUERY EXPANSION);
SHOW PARSE_TREE SELECT cast('30' as time(2)), cast(30 as decimal(6, 3)),
  cast(30 as decimal), cast(30 as float), cast('30' as char(40) charset utf8mb3),
  cast(30 as decimal array);

# Case
SHOW PARSE_TREE SELECT CASE a WHEN b THEN c WHEN d THEN e ELSE f END,
                       CASE a WHEN b THEN c END,
                       CASE WHEN a THEN b ELSE c END;
# Recursive case
SHOW PARSE_TREE SELECT
       CASE a
       WHEN
          CASE WHEN a1 THEN b2 END
       THEN c
       ELSE f END;

SHOW PARSE_TREE SELECT 1 union (select 2 intersect ALL select 3 limit 2 offset 10) limit 1, 5;
SHOW PARSE_TREE select SQL_SMALL_RESULT STRAIGHT_JOIN SQL_BUFFER_RESULT 1 and 2 and 3;
SHOW PARSE_TREE WITH der(col1, col2) AS (SELECT 1, 2), der1 AS (SELECT 2) SELECT * FROM der;
SHOW PARSE_TREE SELECT * FROM t STRAIGHT_JOIN t2 USING (col1, col2);
SHOW PARSE_TREE SELECT * FROM t RIGHT OUTER JOIN t3 on col1/col2;
SHOW PARSE_TREE SELECT * FROM LATERAL (SELECT * FROM mydb.bar bar_alias) AS t(i, j);
SHOW PARSE_TREE SELECT * FROM t JOIN t2 JOIN t3 JOIN t4;
SHOW PARSE_TREE SELECT * FROM t JOIN t2 JOIN (t3 JOIN t4);
SHOW PARSE_TREE SELECT * FROM t JOIN t2 NATURAL JOIN t3 LEFT JOIN t4 USING (col1, col2);
SHOW PARSE_TREE SELECT * FROM t JOIN t2 NATURAL JOIN (t3 LEFT JOIN t4 USING (col1, col2));
SHOW PARSE_TREE SELECT * FROM t JOIN t2 JOIN t3 USING (col1, col2) JOIN (SELECT * FROM tab) AS t4;

# Charsets.
SHOW PARSE_TREE SELECT db.func(), char(col1), char(col1 USING utf8mb4), concat(a,b), concat(a,b) COLLATE utf8mb4_turkish_ci;

--error ER_NOT_SUPPORTED_YET
SHOW PARSE_TREE UPDATE tab SET O = 1;
--error ER_NOT_SUPPORTED_YET
SHOW PARSE_TREE CREATE TABLE tab(id INT);
--error ER_PARSE_ERROR
SHOW PARSE_TREE CREATE TABLE tab(id INVALID_SYNTAX);

--echo #
--echo # Bug#35964157 mysql 8.1.0/8.2.0, mysqld got signal 11,
--echo #              when send sql, show parse_tre
--echo #

--error ER_NOT_SUPPORTED_YET
SHOW PARSE_TREE SET @b:=(SELECT a FROM t);

--error ER_NOT_SUPPORTED_YET
SHOW PARSE_TREE CREATE SCHEMA s1;
