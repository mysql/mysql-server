#
# WL#8697: Invisible Indexes
#
SET SESSION information_schema_stats_expiry=0;
# Test of ALTER INDEX syntax.
CREATE TABLE t1 ( a INT, KEY (a) );
SHOW KEYS FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	a	1	a	A	NULL	NULL	NULL	YES	BTREE			YES	NULL
ALTER TABLE t1 ALTER INDEX a INVISIBLE;
SHOW KEYS FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	a	1	a	A	NULL	NULL	NULL	YES	BTREE			NO	NULL
ALTER TABLE t1 ALTER INDEX a VISIBLE;
SHOW KEYS FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	a	1	a	A	NULL	NULL	NULL	YES	BTREE			YES	NULL
DROP TABLE t1;
# Test of CREATE INDEX syntax with invisible indexes.
CREATE TABLE t1 ( a INT, b INT );
CREATE INDEX a_invisible ON t1(a) INVISIBLE;
CREATE INDEX b_visible ON t1(a) VISIBLE;
Warnings:
Warning	1831	Duplicate index 'b_visible' defined on the table 'test.t1'. This is deprecated and will be disallowed in a future release.
CREATE INDEX a_b_invisible ON t1(a, b) INVISIBLE;
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	a_invisible	1	a	A	NULL	NULL	NULL	YES	BTREE			NO	NULL
t1	1	b_visible	1	a	A	NULL	NULL	NULL	YES	BTREE			YES	NULL
t1	1	a_b_invisible	1	a	A	NULL	NULL	NULL	YES	BTREE			NO	NULL
t1	1	a_b_invisible	2	b	A	NULL	NULL	NULL	YES	BTREE			NO	NULL
DROP TABLE t1;
# Test that invisible indexes are not used.
CREATE TABLE t1 ( a INT, KEY (a) );
CREATE TABLE t2 ( a INT, KEY (a) INVISIBLE );
INSERT INTO t1 VALUES (1), (2), (3), (4), (5);
INSERT INTO t2 SELECT * FROM t1;
ANALYZE TABLE t1, t2;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
test.t2	analyze	status	Table is already up to date
EXPLAIN SELECT a FROM t1;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	index	NULL	a	5	NULL	5	100.00	Using index
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1`
ALTER TABLE t1 ALTER INDEX a INVISIBLE;
EXPLAIN SELECT a FROM t1;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	ALL	NULL	NULL	NULL	NULL	5	100.00	NULL
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1`
ALTER TABLE t1 ALTER INDEX a VISIBLE;
EXPLAIN SELECT a FROM t1;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	index	NULL	a	5	NULL	5	100.00	Using index
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1`
EXPLAIN SELECT a FROM t2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t2	NULL	ALL	NULL	NULL	NULL	NULL	5	100.00	NULL
Warnings:
Note	1003	/* select#1 */ select `test`.`t2`.`a` AS `a` from `test`.`t2`
ALTER TABLE t2 ALTER INDEX a VISIBLE;
EXPLAIN SELECT a FROM t2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t2	NULL	index	NULL	a	5	NULL	5	100.00	Using index
Warnings:
Note	1003	/* select#1 */ select `test`.`t2`.`a` AS `a` from `test`.`t2`
DROP TABLE t1, t2;
# Test that renaming an index does not change visibility and vice versa.
CREATE TABLE t1 (
a INT, INDEX (a),
b INT, INDEX (b) INVISIBLE
);
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	a	1	a	A	NULL	NULL	NULL	YES	BTREE			YES	NULL
t1	1	b	1	b	A	NULL	NULL	NULL	YES	BTREE			NO	NULL
ALTER TABLE t1 RENAME INDEX a TO a1;
ALTER TABLE t1 RENAME INDEX b TO b1;
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	a1	1	a	A	NULL	NULL	NULL	YES	BTREE			YES	NULL
t1	1	b1	1	b	A	NULL	NULL	NULL	YES	BTREE			NO	NULL
ALTER TABLE t1 ALTER INDEX a1 INVISIBLE;
ALTER TABLE t1 ALTER INDEX b1 VISIBLE;
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	a1	1	a	A	NULL	NULL	NULL	YES	BTREE			NO	NULL
t1	1	b1	1	b	A	NULL	NULL	NULL	YES	BTREE			YES	NULL
DROP TABLE t1;
# Test of SHOW CREATE TABLE.
CREATE TABLE t1 (
a INT,
b INT,
c INT,
INDEX (a) VISIBLE,
INDEX (b) INVISIBLE,
INDEX (c)
);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` int DEFAULT NULL,
  `b` int DEFAULT NULL,
  `c` int DEFAULT NULL,
  KEY `a` (`a`),
  KEY `b` (`b`) /*!80000 INVISIBLE */,
  KEY `c` (`c`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t1;
# Test that primary key indexes can't be made invisible.
CREATE TABLE t1 ( a INT, PRIMARY KEY (a) INVISIBLE );
ERROR HY000: A primary key index cannot be invisible
ALTER TABLE t1 ALTER INDEX PRIMARY INVISIBLE;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'PRIMARY INVISIBLE' at line 1
ALTER TABLE t1 ADD PRIMARY KEY (a) INVISIBLE;
ERROR HY000: A primary key index cannot be invisible
#
# Currently we allow to name the primary key index, but the name is
# silently changed to PRIMARY. If this behavior changes in the future,
# we need to take some additional measures to rule out invisible primary
# key indexes. The below two tests serve as triggers for such a change.
#
CREATE TABLE t1( a INT );
ALTER TABLE t1 ADD CONSTRAINT pk PRIMARY KEY (a);
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	0	PRIMARY	1	a	A	0	NULL	NULL		BTREE			YES	NULL
DROP TABLE t1;
CREATE TABLE t1( a INT, PRIMARY KEY pk (a) );
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	0	PRIMARY	1	a	A	0	NULL	NULL		BTREE			YES	NULL
DROP TABLE t1;
CREATE TABLE t1 (
a INT, KEY (a),
b INT, KEY (b) INVISIBLE
);
ALTER TABLE t1 RENAME INDEX no_such_index TO x;
ERROR 42000: Key 'no_such_index' doesn't exist in table 't1'
ALTER TABLE t1 ALTER INDEX no_such_index INVISIBLE;
ERROR 42000: Key 'no_such_index' doesn't exist in table 't1'
#
# Repeated alter actions. Should work.
#
ALTER TABLE t1 ALTER INDEX a INVISIBLE, ALTER INDEX a VISIBLE;
ALTER TABLE t1 ALTER INDEX a INVISIBLE, ALTER INDEX b VISIBLE;
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	a	1	a	A	NULL	NULL	NULL	YES	BTREE			NO	NULL
t1	1	b	1	b	A	NULL	NULL	NULL	YES	BTREE			YES	NULL
#
# Various combinations of RENAME INDEX and ALTER INDEX ... INVISIBLE.
#
ALTER TABLE t1 RENAME INDEX a TO x, RENAME INDEX x TO a;
ERROR 42000: Key 'x' doesn't exist in table 't1'
ALTER TABLE t1 RENAME INDEX a TO x, ALTER INDEX x INVISIBLE;
ERROR 42000: Key 'x' doesn't exist in table 't1'
ALTER TABLE t1 RENAME INDEX a TO x, ALTER INDEX a VISIBLE;
ERROR 42000: Key 'a' doesn't exist in table 't1'
ALTER TABLE t1 ALTER INDEX a VISIBLE, RENAME INDEX a TO x;
ERROR 42000: Key 'a' doesn't exist in table 't1'
#
# Various algorithms and their effects.
#
INSERT INTO t1 VALUES (1, 1), (2, 2), (3, 3);
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
ALTER TABLE t1 ALTER INDEX a INVISIBLE, ALGORITHM = COPY;
affected rows: 3
info: Records: 3  Duplicates: 0  Warnings: 0
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	a	1	a	A	3	NULL	NULL	YES	BTREE			NO	NULL
t1	1	b	1	b	A	3	NULL	NULL	YES	BTREE			YES	NULL
ALTER TABLE t1 ALTER INDEX a VISIBLE, ALGORITHM = INPLACE;
affected rows: 0
info: Records: 0  Duplicates: 0  Warnings: 0
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	Table is already up to date
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	a	1	a	A	3	NULL	NULL	YES	BTREE			YES	NULL
t1	1	b	1	b	A	3	NULL	NULL	YES	BTREE			YES	NULL
ALTER TABLE t1 ALTER INDEX a INVISIBLE, ALGORITHM = DEFAULT;
affected rows: 0
info: Records: 0  Duplicates: 0  Warnings: 0
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	Table is already up to date
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	a	1	a	A	3	NULL	NULL	YES	BTREE			NO	NULL
t1	1	b	1	b	A	3	NULL	NULL	YES	BTREE			YES	NULL
ALTER TABLE t1 ALTER INDEX a VISIBLE;
affected rows: 0
info: Records: 0  Duplicates: 0  Warnings: 0
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	Table is already up to date
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	a	1	a	A	3	NULL	NULL	YES	BTREE			YES	NULL
t1	1	b	1	b	A	3	NULL	NULL	YES	BTREE			YES	NULL
ALTER TABLE t1 ADD INDEX ab(a, b), ALTER INDEX ab INVISIBLE;
ERROR 42000: Key 'ab' doesn't exist in table 't1'
DROP TABLE t1;
#
# Test that constraints implemented by the indexes are still enforced
# while the index is invisible.
#
CREATE TABLE t1 ( a INT, UNIQUE KEY (a) INVISIBLE );
CREATE TABLE t2 ( a INT UNIQUE );
CREATE TABLE t3 ( a INT NOT NULL, KEY (a) INVISIBLE, b INT PRIMARY KEY );
CREATE TABLE t4 ( a INT NOT NULL, UNIQUE KEY (a) INVISIBLE,
b INT PRIMARY KEY AUTO_INCREMENT );
CREATE TABLE t5 ( a INT, b INT, c INT, UNIQUE KEY (a, b, c) INVISIBLE );
INSERT INTO t1 VALUES (1);
INSERT INTO t1 VALUES (1);
ERROR 23000: Duplicate entry '1' for key 't1.a'
ALTER TABLE t2 ALTER INDEX a INVISIBLE;
INSERT INTO t2 VALUES (1);
INSERT INTO t2 VALUES (1);
ERROR 23000: Duplicate entry '1' for key 't2.a'
INSERT INTO t3(a) VALUES (NULL);
ERROR 23000: Column 'a' cannot be null
INSERT INTO t4(a) VALUES (NULL);
ERROR 23000: Column 'a' cannot be null
INSERT INTO t4(a) VALUES (1);
INSERT INTO t4(a) VALUES (1);
ERROR 23000: Duplicate entry '1' for key 't4.a'
INSERT INTO t5 VALUES (1, 2, 3);
INSERT INTO t5 VALUES (1, 2, 3);
ERROR 23000: Duplicate entry '1-2-3' for key 't5.a'
DROP TABLE t1, t2, t3, t4, t5;
#
# Bug#23256900: WL#8697: ASSERTION
# `TABLE_SHARE->IS_MISSING_PRIMARY_KEY() ...` FAILED.
#
# Test for when an index is implicitly promoted to primary key index.
# The first NOT NULL UNIQUE index is candidate for promotion.
# These indexes can't be invisible, either.
CREATE TABLE t1 ( a INT NOT NULL, UNIQUE KEY (a) );
ALTER TABLE t1 ALTER INDEX a INVISIBLE;
ERROR HY000: A primary key index cannot be invisible
EXPLAIN
SELECT * FROM t1;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	no matching row in const table
Warnings:
Note	1003	/* select#1 */ select NULL AS `a` from `test`.`t1`
SELECT * FROM t1;
a
DROP TABLE t1;
# The first NOT NULL UNIQUE index may of course be invisible if it is
# not promoted.
CREATE TABLE t1 (
a INT NOT NULL,
b INT NOT NULL PRIMARY KEY,
UNIQUE KEY (a) INVISIBLE
);
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	0	PRIMARY	1	b	A	0	NULL	NULL		BTREE			YES	NULL
t1	0	a	1	a	A	0	NULL	NULL		BTREE			NO	NULL
DROP TABLE t1;
# The check above applies only to the first NOT NULL UNIQUE index.
CREATE TABLE t1 (
a INT NOT NULL,
b INT NOT NULL,
UNIQUE KEY (a),
UNIQUE KEY (b) INVISIBLE
);
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	0	a	1	a	A	0	NULL	NULL		BTREE			YES	NULL
t1	0	b	1	b	A	0	NULL	NULL		BTREE			NO	NULL
DROP TABLE t1;
CREATE TABLE t1 ( a INT NOT NULL, UNIQUE KEY (a) INVISIBLE );
ERROR HY000: A primary key index cannot be invisible
#
# Bug#23264435: WL#8697: FULLTEXT INDEXES CANNOT BE MADE INVISIBLE
#
CREATE TABLE t1 ( a INT NOT NULL, KEY (a) INVISIBLE, b INT NOT NULL UNIQUE );
CREATE TABLE t2 ( a INT PRIMARY KEY, b INT, INDEX(b) INVISIBLE);
ALTER TABLE t2 ALTER INDEX b VISIBLE;
DROP TABLE t1, t2;
CREATE TEMPORARY TABLE t1 ( a INT, KEY (a) INVISIBLE );
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	a	1	a	A	NULL	NULL	NULL	YES	BTREE			NO	NULL
EXPLAIN SELECT a FROM t1;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	no matching row in const table
Warnings:
Note	1003	/* select#1 */ select NULL AS `a` from `test`.`t1`
DROP TABLE t1;
#
# Invisible spatial indexes.
#
CREATE TABLE t1 (
fid INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
g GEOMETRY NOT NULL SRID 0,
SPATIAL KEY(g)
);
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	Table is already up to date
# There appears to be a bug causing the cardinality number to fluctuate
# for spatial indexes.
EXPLAIN SELECT *
FROM t1
WHERE ST_Within(g,
ST_GeomFromText('Polygon((140 140,160 140,160 160,140 140))'));
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	range	g	g	34	NULL	X	100.00	Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`fid` AS `fid`,`test`.`t1`.`g` AS `g` from `test`.`t1` where st_within(`test`.`t1`.`g`,<cache>(st_geomfromtext('Polygon((140 140,160 140,160 160,140 140))')))
ALTER TABLE t1 ALTER INDEX g INVISIBLE;
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	0	PRIMARY	1	fid	A	X	NULL	NULL		BTREE			YES	NULL
t1	1	g	1	g	A	X	32	NULL		SPATIAL			NO	NULL
EXPLAIN SELECT *
FROM t1
WHERE ST_Within(g,
ST_GeomFromText('Polygon((140 140,160 140,160 160,140 140))'));
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	ALL	NULL	NULL	NULL	NULL	X	100.00	Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`fid` AS `fid`,`test`.`t1`.`g` AS `g` from `test`.`t1` where st_within(`test`.`t1`.`g`,<cache>(st_geomfromtext('Polygon((140 140,160 140,160 160,140 140))')))
DROP TABLE t1;
#
# Test of invisible fulltext indexes.
#
CREATE TABLE t1 (a VARCHAR(200), b TEXT, FULLTEXT (a,b));
INSERT INTO t1 VALUES('Some data', 'for full-text search');
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
EXPLAIN SELECT * FROM t1 WHERE MATCH(a, b) AGAINST ("collections");
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	fulltext	a	a	0	const	1	100.00	Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a`,`test`.`t1`.`b` AS `b` from `test`.`t1` where (match `test`.`t1`.`a`,`test`.`t1`.`b` against ('collections'))
EXPLAIN SELECT * FROM t1 WHERE MATCH(a, b) AGAINST ("collections" IN BOOLEAN MODE);
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	fulltext	a	a	0	const	1	100.00	Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a`,`test`.`t1`.`b` AS `b` from `test`.`t1` where (match `test`.`t1`.`a`,`test`.`t1`.`b` against ('collections' in boolean mode))
ALTER TABLE t1 ALTER INDEX a INVISIBLE;
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	a	1	a	NULL	1	NULL	NULL	YES	FULLTEXT			NO	NULL
t1	1	a	2	b	NULL	1	NULL	NULL	YES	FULLTEXT			NO	NULL
EXPLAIN SELECT * FROM t1 WHERE MATCH(a, b) AGAINST ("collections");
ERROR HY000: Can't find FULLTEXT index matching the column list
EXPLAIN SELECT * FROM t1 WHERE MATCH(a, b) AGAINST ("collections" IN BOOLEAN MODE);
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a`,`test`.`t1`.`b` AS `b` from `test`.`t1` where (match `test`.`t1`.`a`,`test`.`t1`.`b` against ('collections' in boolean mode))
SELECT * FROM t1 WHERE MATCH(a, b) AGAINST ("collections");
ERROR HY000: Can't find FULLTEXT index matching the column list
SELECT * FROM t1 WHERE MATCH(a, b) AGAINST ("collections" IN BOOLEAN MODE);
a	b
DROP TABLE t1;
#
# Invisible indexes on AUTO_INCREMENT columns.
#
CREATE TABLE t1 ( a INT AUTO_INCREMENT, KEY ( a ) );
INSERT INTO t1 VALUES (), (), ();
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
EXPLAIN SELECT a FROM t1;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	index	NULL	a	4	NULL	3	100.00	Using index
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1`
ALTER TABLE t1 ALTER INDEX a INVISIBLE;
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	a	1	a	A	3	NULL	NULL		BTREE			NO	NULL
EXPLAIN SELECT a FROM t1;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	ALL	NULL	NULL	NULL	NULL	3	100.00	NULL
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1`
DROP TABLE t1;
#
# Bug#24660093: REMOVING AN INVISIBLE INDEX BREAKS EXPLAIN
#
CREATE TABLE t1 ( a INT, KEY(a) INVISIBLE );
SELECT * FROM t1 FORCE INDEX(a);
ERROR 42000: Key 'a' doesn't exist in table 't1'
SELECT * FROM t1 FORCE INDEX FOR JOIN (a);
ERROR 42000: Key 'a' doesn't exist in table 't1'
SELECT * FROM t1 FORCE INDEX FOR ORDER BY (a);
ERROR 42000: Key 'a' doesn't exist in table 't1'
SELECT * FROM t1 FORCE INDEX FOR GROUP BY (a);
ERROR 42000: Key 'a' doesn't exist in table 't1'
SELECT * FROM t1 USE INDEX(a);
ERROR 42000: Key 'a' doesn't exist in table 't1'
SELECT * FROM t1 USE INDEX FOR JOIN (a);
ERROR 42000: Key 'a' doesn't exist in table 't1'
SELECT * FROM t1 USE INDEX FOR ORDER BY (a);
ERROR 42000: Key 'a' doesn't exist in table 't1'
SELECT * FROM t1 USE INDEX FOR GROUP BY (a);
ERROR 42000: Key 'a' doesn't exist in table 't1'
SELECT * FROM t1 IGNORE INDEX(a);
ERROR 42000: Key 'a' doesn't exist in table 't1'
SELECT * FROM t1 IGNORE INDEX FOR JOIN (a);
ERROR 42000: Key 'a' doesn't exist in table 't1'
SELECT * FROM t1 IGNORE INDEX FOR ORDER BY (a);
ERROR 42000: Key 'a' doesn't exist in table 't1'
SELECT * FROM t1 IGNORE INDEX FOR GROUP BY (a);
ERROR 42000: Key 'a' doesn't exist in table 't1'
DROP TABLE t1;
