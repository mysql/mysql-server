SET NAMES latin1;
CREATE TABLE t1 (
word VARCHAR(64),
bar INT(11) default 0,
PRIMARY KEY (word))
ENGINE=MyISAM
CHARSET utf32
COLLATE utf32_general_ci ;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
INSERT INTO t1 (word) VALUES ("aar");
INSERT INTO t1 (word) VALUES ("a");
INSERT INTO t1 (word) VALUES ("aardvar");
INSERT INTO t1 (word) VALUES ("aardvark");
INSERT INTO t1 (word) VALUES ("aardvara");
INSERT INTO t1 (word) VALUES ("aardvarz");
EXPLAIN SELECT * FROM t1 ORDER BY word;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	ALL	NULL	NULL	NULL	NULL	6	100.00	Using filesort
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`word` AS `word`,`test`.`t1`.`bar` AS `bar` from `test`.`t1` order by `test`.`t1`.`word`
SELECT * FROM t1 ORDER BY word;
word	bar
a	0
aar	0
aardvar	0
aardvara	0
aardvark	0
aardvarz	0
EXPLAIN SELECT word FROM t1 ORDER BY word;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	index	NULL	PRIMARY	258	NULL	6	100.00	Using index
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`word` AS `word` from `test`.`t1` order by `test`.`t1`.`word`
SELECT word FROM t1 ORDER by word;
word
a
aar
aardvar
aardvara
aardvark
aardvarz
DROP TABLE t1;
CREATE TABLE t1 (
word VARCHAR(64) ,
PRIMARY KEY (word))
ENGINE=MyISAM
CHARSET utf32
COLLATE utf32_general_ci;
INSERT INTO t1 (word) VALUES ("aar");
INSERT INTO t1 (word) VALUES ("a");
INSERT INTO t1 (word) VALUES ("aardvar");
INSERT INTO t1 (word) VALUES ("aardvark");
INSERT INTO t1 (word) VALUES ("aardvara");
INSERT INTO t1 (word) VALUES ("aardvarz");
EXPLAIN SELECT * FROM t1 ORDER BY WORD;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	index	NULL	PRIMARY	258	NULL	6	100.00	Using index
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`word` AS `word` from `test`.`t1` order by `test`.`t1`.`word`
SELECT * FROM t1 ORDER BY word;
word
a
aar
aardvar
aardvara
aardvark
aardvarz
DROP TABLE t1;
CREATE TABLE t1 (
word TEXT,
bar INT(11) AUTO_INCREMENT,
PRIMARY KEY (bar))
ENGINE=MyISAM
CHARSET utf32
COLLATE utf32_general_ci ;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
INSERT INTO t1 (word) VALUES ("aar");
INSERT INTO t1 (word) VALUES ("a" );
INSERT INTO t1 (word) VALUES ("aardvar");
INSERT INTO t1 (word) VALUES ("aardvark");
INSERT INTO t1 (word) VALUES ("aardvara");
INSERT INTO t1 (word) VALUES ("aardvarz");
EXPLAIN SELECT * FROM t1 ORDER BY word;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	ALL	NULL	NULL	NULL	NULL	6	100.00	Using filesort
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`word` AS `word`,`test`.`t1`.`bar` AS `bar` from `test`.`t1` order by `test`.`t1`.`word`
SELECT * FROM t1 ORDER BY word;
word	bar
a	2
aar	1
aardvar	3
aardvara	5
aardvark	4
aardvarz	6
EXPLAIN SELECT word FROM t1 ORDER BY word;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	ALL	NULL	NULL	NULL	NULL	6	100.00	Using filesort
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`word` AS `word` from `test`.`t1` order by `test`.`t1`.`word`
SELECT word FROM t1 ORDER BY word;
word
a
aar
aardvar
aardvara
aardvark
aardvarz
DROP TABLE t1;
CREATE TABLE t1 (
a varchar(250) NOT NULL default '',
KEY a (a)
) ENGINE=MyISAM DEFAULT CHARSET=utf32 COLLATE utf32_general_ci;
insert into t1 values (0x803d);
insert into t1 values (0x005b);
select hex(a) from t1;
hex(a)
0000005B
0000803D
drop table t1;
create table t1 (
a char(10) character set utf32 not null,
index a (a)
) engine=myisam;
insert into t1 values (repeat(0x0000201f, 10));
insert into t1 values (repeat(0x00002020, 10));
insert into t1 values (repeat(0x00002021, 10));
explain select hex(a) from t1 order by a;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	index	NULL	a	40	NULL	3	100.00	Using index
Warnings:
Note	1003	/* select#1 */ select hex(`test`.`t1`.`a`) AS `hex(a)` from `test`.`t1` order by `test`.`t1`.`a`
select hex(a) from t1 order by a;
hex(a)
0000201F0000201F0000201F0000201F0000201F0000201F0000201F0000201F0000201F0000201F
00002020000020200000202000002020000020200000202000002020000020200000202000002020
00002021000020210000202100002021000020210000202100002021000020210000202100002021
alter table t1 drop index a;
select hex(a) from t1 order by a;
hex(a)
0000201F0000201F0000201F0000201F0000201F0000201F0000201F0000201F0000201F0000201F
00002020000020200000202000002020000020200000202000002020000020200000202000002020
00002021000020210000202100002021000020210000202100002021000020210000202100002021
drop table t1;
create table t1 (a varchar(250) character set utf32 primary key) engine=MyISAM;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` varchar(250) CHARACTER SET utf32 COLLATE utf32_general_ci NOT NULL,
  PRIMARY KEY (`a`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
drop table t1;
create table t1 (a varchar(334) character set utf32 primary key) engine=MyISAM;
ERROR 42000: Specified key was too long; max key length is 1000 bytes
#
# Bug#42511 mysqld: ctype-ucs2.c:2044: my_strnncollsp_utf32: Assertion (tlen % 4) == 0' failed
#
CREATE TABLE t1 (
b char(250) CHARACTER SET utf32,
key (b)
) ENGINE=MYISAM;
INSERT INTO t1 VALUES ('d'),('f');
SELECT * FROM t1 WHERE b BETWEEN 'a' AND 'z';
b
d
f
DROP TABLE t1;
