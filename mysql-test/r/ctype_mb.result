drop table if exists t1;
CREATE TABLE t1 SELECT _utf8'test' as c1, _utf8'тест' as c2;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` varchar(4) CHARACTER SET utf8mb3 NOT NULL DEFAULT '',
  `c2` varchar(4) CHARACTER SET utf8mb3 NOT NULL DEFAULT ''
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DELETE FROM t1;
ALTER TABLE t1 ADD c3 CHAR(4) CHARACTER SET utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` varchar(4) CHARACTER SET utf8mb3 NOT NULL DEFAULT '',
  `c2` varchar(4) CHARACTER SET utf8mb3 NOT NULL DEFAULT '',
  `c3` char(4) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
INSERT IGNORE INTO t1 VALUES ('aaaabbbbccccdddd','aaaabbbbccccdddd','aaaabbbbccccdddd');
Warnings:
Warning	1265	Data truncated for column 'c1' at row 1
Warning	1265	Data truncated for column 'c2' at row 1
Warning	1265	Data truncated for column 'c3' at row 1
SELECT * FROM t1;
c1	c2	c3
aaaa	aaaa	aaaa
DROP TABLE t1;
CREATE TABLE t1 (a CHAR(4) CHARACTER SET utf8, KEY key_a(a(3)));
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` char(4) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci DEFAULT NULL,
  KEY `key_a` (`a`(3))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SHOW KEYS FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	key_a	1	a	A	0	3	NULL	YES	BTREE			YES	NULL
ALTER TABLE t1 CHANGE a a CHAR(4);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` char(4) DEFAULT NULL,
  KEY `key_a` (`a`(3))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SHOW KEYS FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	key_a	1	a	A	0	3	NULL	YES	BTREE			YES	NULL
ALTER TABLE t1 CHANGE a a CHAR(4) CHARACTER SET utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` char(4) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci DEFAULT NULL,
  KEY `key_a` (`a`(3))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SHOW KEYS FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	key_a	1	a	A	0	3	NULL	YES	BTREE			YES	NULL
DROP TABLE t1;
