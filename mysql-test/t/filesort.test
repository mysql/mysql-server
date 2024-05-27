--source include/elide_costs.inc

--echo #
--echo # Bug#24709595 WL#8741: ASSERTION `SORTORDER->NEED_STRXNFRM == 0' FAILED
--echo #

CREATE TABLE t1(
  c VARCHAR(10) CHARACTER SET utf8mb3 DEFAULT NULL
);
CREATE TABLE t2(
  c1 VARCHAR(255) CHARACTER SET utf8mb3 DEFAULT NULL,
  c2 VARCHAR(10) CHARACTER SET utf8mb3 DEFAULT NULL
);

INSERT INTO t1 VALUES('x');
INSERT INTO t2 VALUES('g','if'), ('not','ojgygqcgqi');

SELECT * FROM t2 WHERE (c2) <> (SELECT MAX(c) FROM t1 GROUP BY c1);

DROP TABLE t1, t2;

--echo #
--echo # Bug#29660945: SIG 6 AT TEMPTABLE::HANDLER::POSITION | SRC/HANDLER.CC
--echo #

SET optimizer_switch='block_nested_loop=off';

CREATE TABLE t1 (
  f1 varchar(255),
  f2 varchar(255)
);

INSERT INTO t1 VALUES (NULL,'A');
INSERT INTO t1 VALUES ('A',NULL);

SELECT * FROM t1 AS alias1 JOIN t1 AS alias2 ON alias1.f1=alias2.f2 ORDER BY alias2.f1 LIMIT 50;

DROP TABLE t1;

SET optimizer_switch=DEFAULT;

# Verify that we can sort data with large blobs (larger than the sort buffer).

SET sort_buffer_size=32768;

CREATE TABLE t1 (
  f1 INTEGER,
  f2 LONGBLOB
);

INSERT INTO t1 VALUES ( 2, REPEAT('-', 1048576) );
INSERT INTO t1 VALUES ( 1, REPEAT('x', 1048576) );

--replace_regex /x{1024}/x{1024}/ /-{1024}/-{1024}/
SELECT * FROM t1 ORDER BY f1;

DROP TABLE t1;
SET sort_buffer_size=DEFAULT;

--echo #
--echo # Bug #30687020: ORDER BY DOES NOT WORK FOR A COLUMN WITH A STRING FUNCTION AND UTF8MB4_0900_AI_C
--echo #

CREATE TABLE t1 ( f1 VARCHAR(100) );
INSERT INTO t1 VALUES (''), (NULL), (''), (NULL);
SELECT CONCAT(f1, '') AS dummy FROM t1 ORDER BY dummy;
DROP TABLE t1;

--echo #
--echo # Bug #30791074: WEIGHT_STRING: ASSERTION `0' FAILED. IN TIME_ZONE_UTC::TIME_TO_GMT_SEC
--echo #

CREATE TABLE t1 ( f1 INTEGER );
INSERT INTO t1 VALUES (0), (1), (101);
SELECT * FROM t1 ORDER BY UNIX_TIMESTAMP(f1);
DROP TABLE t1;

--echo #
--echo # Bug #31397840: Wrong result of SELECT DISTINCT JSON_ARRAYAGG
--echo #

SET sql_mode='';
CREATE TABLE t1
  (
     id            INT PRIMARY KEY,
     char_field    CHAR(10)
  );
INSERT INTO t1 VALUES (580801, '580801');
INSERT INTO t1 VALUES (580901, '580901');
INSERT INTO t1 VALUES (581001, '581001');
INSERT INTO t1 VALUES (581101, '581101');

ANALYZE TABLE t1;

let $query = SELECT SQL_BIG_RESULT DISTINCT JSON_ARRAYAGG(char_field), JSON_ARRAYAGG(2) FROM t1 GROUP BY id ORDER BY id;
eval $query;
--replace_regex $elide_metrics
--skip_if_hypergraph  # Uses an index scan instead of a sort.
eval EXPLAIN FORMAT=tree $query;

DROP TABLE t1;
SET sql_mode=DEFAULT;

--echo #
--echo # Bug #31588831: ASSERTION FAILURE IN NEWWEEDOUTACCESSPATHFORTABLES() AT SQL/SQL_EXECUTOR.CC
--echo #

CREATE TABLE t1 (pk INTEGER, f2 INTEGER, PRIMARY KEY (pk));
CREATE TABLE t2 (pk INTEGER, f2 INTEGER, PRIMARY KEY (pk));

INSERT INTO t1 VALUES (1,1), (2,2);
ANALYZE TABLE t1, t2;

# Sets up a filesort on t1 which initially is not part of weedout,
# but gets caught in one anyway later, and thus needs to be sorted by row ID.
--replace_regex $elide_metrics
--skip_if_hypergraph  # Does not support weedout.
EXPLAIN FORMAT=tree SELECT * FROM
  t1
  LEFT JOIN t1 AS table2 ON 119 IN (
    SELECT SUBQUERY1_t2.pk
    FROM t1 AS SUBQUERY1_t1, (t2 AS SUBQUERY1_t2 STRAIGHT_JOIN t1 ON TRUE)
  )
  ORDER BY t1.f2;

DROP TABLE t1, t2;

--echo #
--echo # Bug #32169770: HYPERGRAPH: ASSERTION `M_PART_INFO->IS_PARTITION_USED(M_LAST_PART)' FAILED.
--echo #

# The LONGTEXT is to force a sort by row ID.
# The ... ON FALSE condition is to make sure the Read() call never
# hits the partitioned table, so that it isn't properly initialized
# when filesort wants row IDs.

CREATE TABLE t1 (a LONGTEXT);
INSERT INTO t1 VALUES ('a');
CREATE TABLE t2 (a INTEGER PRIMARY KEY) PARTITION BY key() PARTITIONS 2;
SELECT 1 AS a FROM t1 LEFT JOIN t2 ON FALSE GROUP BY a;
DROP TABLE t1, t2;

--echo #
--echo # Bug #32617876: SIG11 IN SORT_PARAM::MAKE_SORTKEY|SQL/FILESORT.CC WITH HYPERGRAPH
--echo #

CREATE TABLE t1 (
  a LONGBLOB,
  b INTEGER,
  c INTEGER
);

INSERT INTO t1 VALUES (NULL,1,3);
INSERT INTO t1 VALUES (NULL,1,NULL);
ANALYZE TABLE t1;

# Set up a sort by row ID, where a temporary table (belonging to the
# nonmergable derived table) has a NULL-complemented row, but never gets
# properly initialized, and thus does not actually have a row ID.
SELECT *
FROM t1
LEFT JOIN (
  t1 AS t2 JOIN t1 AS t3 ON t2.c = t3.b
  JOIN ( SELECT DISTINCT b FROM t1 ) AS d1
) ON TRUE
ORDER BY t1.c;

DROP TABLE t1;

--echo #
--echo # Bug #32685064: ASSERTION `M_INDEX_CURSOR.IS_POSITIONED()' FAILED WITH HYPERGRAPH
--echo #

CREATE TABLE t1 (a LONGTEXT);
INSERT INTO t1 VALUES ('8');
SELECT 'a' AS f1 FROM t1 WHERE a='8' GROUP BY f1 ORDER BY CONCAT(f1);
DROP TABLE t1;

--echo #
--echo # Bug #33030289 REGRESSION: ASSERTION `REC_SZ <= M_ELEMENT_SIZE' FAILED.
--echo #

CREATE TABLE t(a DECIMAL(55,19) NOT NULL);
INSERT INTO t VALUES(0),(1),(2),(3),(4),(5);
--error ER_GIS_INVALID_DATA
SELECT
(
  SELECT 1 FROM t
  ORDER BY ST_HAUSDORFFDISTANCE(a, ST_ASTEXT(1,'axis-order=lat-long')), a
  LIMIT 1
);
DROP TABLE t;

--echo #
--echo # Bug #32738705: "OUT OF SORT MEMORY ERROR" HAS AN INCONSISTENT RELATIONSHIP WITH THE BUFFER SIZE
--echo #

CREATE TABLE t1 ( a INTEGER, b TEXT );
INSERT INTO t1 VALUES (1, REPEAT('x', 40001));
INSERT INTO t1 VALUES (2, REPEAT('x', 40002));
INSERT INTO t1 VALUES (3, REPEAT('x', 40003));
INSERT INTO t1 VALUES (4, REPEAT('x', 40005));
INSERT INTO t1 VALUES (5, REPEAT('x', 40008));
INSERT INTO t1 VALUES (6, REPEAT('x', 40013));

# Set up a sort with large addons (since b is TEXT and not HUGETEXT,
# it's treated as a packed addon). We can keep one row and then some
# in the sort buffer, but not two, so we need special handling during merging.
SET sort_buffer_size=65536;
SELECT a, LENGTH(b) FROM t1 ORDER BY a DESC;
SET sort_buffer_size=DEFAULT;

DROP TABLE t1;

--echo #
--echo # Bug#36104667: Assertion failure in Sort_param::make_sortkey with
--echo #               hypergraph_optimizer
--echo #

CREATE TABLE t1 (i INT);

INSERT INTO t1 VALUES (), ();

CREATE TABLE t2 (
  pk INT PRIMARY KEY,
  vc VARCHAR(1) NOT NULL,
  gc INT GENERATED ALWAYS AS (1));

INSERT INTO t2 VALUES (1, 'a', DEFAULT), (2, 'b', DEFAULT);

ANALYZE TABLE t1, t2;

# The next query doesn't fail unless we do this first. (It fills the
# vc column in t2's record buffer with garbage.)
SELECT gc FROM t2;

# Used to hit an assertion failure with the hypergraph optimizer.
SELECT 1
FROM t1,
     t2 RIGHT OUTER JOIN t1 AS t3 ON t2.pk = t3.i
WHERE (SELECT 1 FROM t1 WHERE t2.vc = 'x')
ORDER BY t2.vc;

DROP TABLE t1, t2;
