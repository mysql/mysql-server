--echo #
--echo # WL#8697: Invisible Indexes
--echo #

SET SESSION information_schema_stats_expiry=0;

--echo # Test of ALTER INDEX syntax.

CREATE TABLE t1 ( a INT, KEY (a) );
SHOW KEYS FROM t1;
ALTER TABLE t1 ALTER INDEX a INVISIBLE;
SHOW KEYS FROM t1;
ALTER TABLE t1 ALTER INDEX a VISIBLE;
SHOW KEYS FROM t1;
DROP TABLE t1;

--echo # Test of CREATE INDEX syntax with invisible indexes.

CREATE TABLE t1 ( a INT, b INT );
CREATE INDEX a_invisible ON t1(a) INVISIBLE;
CREATE INDEX b_visible ON t1(a) VISIBLE;
CREATE INDEX a_b_invisible ON t1(a, b) INVISIBLE;
SHOW INDEXES FROM t1;
DROP TABLE t1;

--echo # Test that invisible indexes are not used.

CREATE TABLE t1 ( a INT, KEY (a) );
CREATE TABLE t2 ( a INT, KEY (a) INVISIBLE );

INSERT INTO t1 VALUES (1), (2), (3), (4), (5);
INSERT INTO t2 SELECT * FROM t1;

ANALYZE TABLE t1, t2;

EXPLAIN SELECT a FROM t1;
ALTER TABLE t1 ALTER INDEX a INVISIBLE;
EXPLAIN SELECT a FROM t1;
ALTER TABLE t1 ALTER INDEX a VISIBLE;
EXPLAIN SELECT a FROM t1;

EXPLAIN SELECT a FROM t2;
ALTER TABLE t2 ALTER INDEX a VISIBLE;
EXPLAIN SELECT a FROM t2;

DROP TABLE t1, t2;

--echo # Test that renaming an index does not change visibility and vice versa.

CREATE TABLE t1 (
  a INT, INDEX (a),
  b INT, INDEX (b) INVISIBLE
);

SHOW INDEXES FROM t1;

ALTER TABLE t1 RENAME INDEX a TO a1;
ALTER TABLE t1 RENAME INDEX b TO b1;

SHOW INDEXES FROM t1;

ALTER TABLE t1 ALTER INDEX a1 INVISIBLE;
ALTER TABLE t1 ALTER INDEX b1 VISIBLE;

SHOW INDEXES FROM t1;

DROP TABLE t1;

--echo # Test of SHOW CREATE TABLE.

CREATE TABLE t1 (
  a INT,
  b INT,
  c INT,
  INDEX (a) VISIBLE,
  INDEX (b) INVISIBLE,
  INDEX (c)
);

SHOW CREATE TABLE t1;

DROP TABLE t1;

--echo # Test that primary key indexes can't be made invisible.

--error ER_PK_INDEX_CANT_BE_INVISIBLE
CREATE TABLE t1 ( a INT, PRIMARY KEY (a) INVISIBLE );
--error ER_PARSE_ERROR
CREATE TABLE t1 ( a INT PRIMARY KEY INVISIBLE );
--error ER_PARSE_ERROR
CREATE TABLE t1 ( a INT KEY INVISIBLE );
--error ER_PARSE_ERROR
ALTER TABLE t1 ALTER INDEX PRIMARY INVISIBLE;
--error ER_PK_INDEX_CANT_BE_INVISIBLE
ALTER TABLE t1 ADD PRIMARY KEY (a) INVISIBLE;

--echo #
--echo # Currently we allow to name the primary key index, but the name is
--echo # silently changed to PRIMARY. If this behavior changes in the future,
--echo # we need to take some additional measures to rule out invisible primary
--echo # key indexes. The below two tests serve as triggers for such a change.
--echo #
CREATE TABLE t1( a INT );
ALTER TABLE t1 ADD CONSTRAINT pk PRIMARY KEY (a);
SHOW INDEXES FROM t1;
DROP TABLE t1;

CREATE TABLE t1( a INT, PRIMARY KEY pk (a) );
SHOW INDEXES FROM t1;
DROP TABLE t1;

CREATE TABLE t1 (
  a INT, KEY (a),
  b INT, KEY (b) INVISIBLE
);

--error ER_KEY_DOES_NOT_EXITS
ALTER TABLE t1 RENAME INDEX no_such_index TO x;
--error ER_KEY_DOES_NOT_EXITS
ALTER TABLE t1 ALTER INDEX no_such_index INVISIBLE;

--echo #
--echo # Repeated alter actions. Should work.
--echo #
ALTER TABLE t1 ALTER INDEX a INVISIBLE, ALTER INDEX a VISIBLE;
ALTER TABLE t1 ALTER INDEX a INVISIBLE, ALTER INDEX b VISIBLE;
SHOW INDEXES FROM t1;

--echo #
--echo # Various combinations of RENAME INDEX and ALTER INDEX ... INVISIBLE.
--echo #
--error ER_KEY_DOES_NOT_EXITS
ALTER TABLE t1 RENAME INDEX a TO x, RENAME INDEX x TO a;
--error ER_KEY_DOES_NOT_EXITS
ALTER TABLE t1 RENAME INDEX a TO x, ALTER INDEX x INVISIBLE;
--error ER_KEY_DOES_NOT_EXITS
ALTER TABLE t1 RENAME INDEX a TO x, ALTER INDEX a VISIBLE;
--error ER_KEY_DOES_NOT_EXITS
ALTER TABLE t1 ALTER INDEX a VISIBLE, RENAME INDEX a TO x;


--echo #
--echo # Various algorithms and their effects.
--echo #
INSERT INTO t1 VALUES (1, 1), (2, 2), (3, 3);
ANALYZE TABLE t1;

--enable_info
ALTER TABLE t1 ALTER INDEX a INVISIBLE, ALGORITHM = COPY;
--disable_info
ANALYZE TABLE t1;
SHOW INDEXES FROM t1;

--enable_info
ALTER TABLE t1 ALTER INDEX a VISIBLE, ALGORITHM = INPLACE;
--disable_info
ANALYZE TABLE t1;
SHOW INDEXES FROM t1;

--enable_info
ALTER TABLE t1 ALTER INDEX a INVISIBLE, ALGORITHM = DEFAULT;
--disable_info
ANALYZE TABLE t1;
SHOW INDEXES FROM t1;

--enable_info
ALTER TABLE t1 ALTER INDEX a VISIBLE;
--disable_info
ANALYZE TABLE t1;
SHOW INDEXES FROM t1;

--error ER_KEY_DOES_NOT_EXITS
ALTER TABLE t1 ADD INDEX ab(a, b), ALTER INDEX ab INVISIBLE;

DROP TABLE t1;

--echo #
--echo # Test that constraints implemented by the indexes are still enforced
--echo # while the index is invisible.
--echo #
CREATE TABLE t1 ( a INT, UNIQUE KEY (a) INVISIBLE );
CREATE TABLE t2 ( a INT UNIQUE );
CREATE TABLE t3 ( a INT NOT NULL, KEY (a) INVISIBLE, b INT PRIMARY KEY );
CREATE TABLE t4 ( a INT NOT NULL, UNIQUE KEY (a) INVISIBLE,
                  b INT PRIMARY KEY AUTO_INCREMENT );
CREATE TABLE t5 ( a INT, b INT, c INT, UNIQUE KEY (a, b, c) INVISIBLE );

INSERT INTO t1 VALUES (1);
--error ER_DUP_ENTRY
INSERT INTO t1 VALUES (1);

ALTER TABLE t2 ALTER INDEX a INVISIBLE;
INSERT INTO t2 VALUES (1);
--error ER_DUP_ENTRY
INSERT INTO t2 VALUES (1);

--error ER_BAD_NULL_ERROR
INSERT INTO t3(a) VALUES (NULL);

--error ER_BAD_NULL_ERROR
INSERT INTO t4(a) VALUES (NULL);
INSERT INTO t4(a) VALUES (1);
--error ER_DUP_ENTRY
INSERT INTO t4(a) VALUES (1);

INSERT INTO t5 VALUES (1, 2, 3);
--error ER_DUP_ENTRY
INSERT INTO t5 VALUES (1, 2, 3);

DROP TABLE t1, t2, t3, t4, t5;

--echo #
--echo # Bug#23256900: WL#8697: ASSERTION
--echo # `TABLE_SHARE->IS_MISSING_PRIMARY_KEY() ...` FAILED.
--echo #

--echo # Test for when an index is implicitly promoted to primary key index.
--echo # The first NOT NULL UNIQUE index is candidate for promotion.
--echo # These indexes can't be invisible, either.
CREATE TABLE t1 ( a INT NOT NULL, UNIQUE KEY (a) );
--error ER_PK_INDEX_CANT_BE_INVISIBLE
ALTER TABLE t1 ALTER INDEX a INVISIBLE;
EXPLAIN
SELECT * FROM t1;
SELECT * FROM t1;
DROP TABLE t1;

--echo # The first NOT NULL UNIQUE index may of course be invisible if it is
--echo # not promoted.
CREATE TABLE t1 (
  a INT NOT NULL,
  b INT NOT NULL PRIMARY KEY,
  UNIQUE KEY (a) INVISIBLE
);
SHOW INDEXES FROM t1;
DROP TABLE t1;

--echo # The check above applies only to the first NOT NULL UNIQUE index.
CREATE TABLE t1 (
  a INT NOT NULL,
  b INT NOT NULL,
  UNIQUE KEY (a),
  UNIQUE KEY (b) INVISIBLE
);
SHOW INDEXES FROM t1;
DROP TABLE t1;

--error ER_PK_INDEX_CANT_BE_INVISIBLE
CREATE TABLE t1 ( a INT NOT NULL, UNIQUE KEY (a) INVISIBLE );

--echo #
--echo # Bug#23264435: WL#8697: FULLTEXT INDEXES CANNOT BE MADE INVISIBLE
--echo #
CREATE TABLE t1 ( a INT NOT NULL, KEY (a) INVISIBLE, b INT NOT NULL UNIQUE );
CREATE TABLE t2 ( a INT PRIMARY KEY, b INT, INDEX(b) INVISIBLE);

ALTER TABLE t2 ALTER INDEX b VISIBLE;
DROP TABLE t1, t2;

CREATE TEMPORARY TABLE t1 ( a INT, KEY (a) INVISIBLE );
SHOW INDEXES FROM t1;
EXPLAIN SELECT a FROM t1;
DROP TABLE t1;


--echo #
--echo # Invisible spatial indexes.
--echo #
CREATE TABLE t1 (
  fid INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  g GEOMETRY NOT NULL SRID 0,
  SPATIAL KEY(g)
);

ANALYZE TABLE t1;

--disable_query_log
let $1=150;
let $2=150;
while ($1)
{
  eval INSERT INTO t1 (g) VALUES (ST_GeomFromText('LineString($1 $1, $2 $2)'));
  dec $1;
  inc $2;
}
--enable_query_log

let $query=
EXPLAIN SELECT *
FROM t1
WHERE ST_Within(g,
                ST_GeomFromText('Polygon((140 140,160 140,160 160,140 140))'));

--echo # There appears to be a bug causing the cardinality number to fluctuate
--echo # for spatial indexes.
--replace_column 10 X
--eval $query
ALTER TABLE t1 ALTER INDEX g INVISIBLE;
--replace_column 7 X
SHOW INDEXES FROM t1;
--replace_column 10 X
--eval $query

DROP TABLE t1;


--echo #
--echo # Test of invisible fulltext indexes.
--echo #
CREATE TABLE t1 (a VARCHAR(200), b TEXT, FULLTEXT (a,b));
INSERT INTO t1 VALUES('Some data', 'for full-text search');
ANALYZE TABLE t1;

let $query1= SELECT * FROM t1 WHERE MATCH(a, b) AGAINST ("collections");
let $query2=
SELECT * FROM t1 WHERE MATCH(a, b) AGAINST ("collections" IN BOOLEAN MODE);

--eval EXPLAIN $query1
--eval EXPLAIN $query2
ALTER TABLE t1 ALTER INDEX a INVISIBLE;
SHOW INDEXES FROM t1;
--disable_abort_on_error
--eval EXPLAIN $query1
--eval EXPLAIN $query2
--eval $query1
--eval $query2
--enable_abort_on_error

DROP TABLE t1;


--echo #
--echo # Invisible indexes on AUTO_INCREMENT columns.
--echo #
CREATE TABLE t1 ( a INT AUTO_INCREMENT, KEY ( a ) );
INSERT INTO t1 VALUES (), (), ();
ANALYZE TABLE t1;

EXPLAIN SELECT a FROM t1;
ALTER TABLE t1 ALTER INDEX a INVISIBLE;
SHOW INDEXES FROM t1;
EXPLAIN SELECT a FROM t1;

DROP TABLE t1;

--echo #
--echo # Bug#24660093: REMOVING AN INVISIBLE INDEX BREAKS EXPLAIN
--echo #
CREATE TABLE t1 ( a INT, KEY(a) INVISIBLE );

--error ER_KEY_DOES_NOT_EXITS
SELECT * FROM t1 FORCE INDEX(a);

--error ER_KEY_DOES_NOT_EXITS
SELECT * FROM t1 FORCE INDEX FOR JOIN (a);

--error ER_KEY_DOES_NOT_EXITS
SELECT * FROM t1 FORCE INDEX FOR ORDER BY (a);

--error ER_KEY_DOES_NOT_EXITS
SELECT * FROM t1 FORCE INDEX FOR GROUP BY (a);


--error ER_KEY_DOES_NOT_EXITS
SELECT * FROM t1 USE INDEX(a);

--error ER_KEY_DOES_NOT_EXITS
SELECT * FROM t1 USE INDEX FOR JOIN (a);

--error ER_KEY_DOES_NOT_EXITS
SELECT * FROM t1 USE INDEX FOR ORDER BY (a);

--error ER_KEY_DOES_NOT_EXITS
SELECT * FROM t1 USE INDEX FOR GROUP BY (a);


--error ER_KEY_DOES_NOT_EXITS
SELECT * FROM t1 IGNORE INDEX(a);

--error ER_KEY_DOES_NOT_EXITS
SELECT * FROM t1 IGNORE INDEX FOR JOIN (a);

--error ER_KEY_DOES_NOT_EXITS
SELECT * FROM t1 IGNORE INDEX FOR ORDER BY (a);

--error ER_KEY_DOES_NOT_EXITS
SELECT * FROM t1 IGNORE INDEX FOR GROUP BY (a);


DROP TABLE t1;
