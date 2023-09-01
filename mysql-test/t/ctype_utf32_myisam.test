--source include/force_myisam_default.inc
--source include/have_myisam.inc

#######################################################

#
# Bug 1264
#
# Description:
#
# When using a ucs2 table in MySQL,
# either with ucs2_general_ci or ucs2_bin collation,
# words are returned in an incorrect order when using ORDER BY
# on an _indexed_ CHAR or VARCHAR column. They are sorted with
# the longest word *first* instead of last. I.E. The word "aardvark"
# is in the results before the word "a".
#
# If there is no index for the column, the problem does not occur.
#
# Interestingly, if there is no second column, the words are returned
# in the correct order.
#
# According to EXPLAIN, it looks like when the output includes columns that
# are not part of the index sorted on, it does a filesort, which fails.
# Using a straight index yields correct results.

SET NAMES latin1;

#
# Two fields, index
#

CREATE TABLE t1 (
   word VARCHAR(64),
   bar INT(11) default 0,
   PRIMARY KEY (word))
   ENGINE=MyISAM
   CHARSET utf32
   COLLATE utf32_general_ci ;

INSERT INTO t1 (word) VALUES ("aar");
INSERT INTO t1 (word) VALUES ("a");
INSERT INTO t1 (word) VALUES ("aardvar");
INSERT INTO t1 (word) VALUES ("aardvark");
INSERT INTO t1 (word) VALUES ("aardvara");
INSERT INTO t1 (word) VALUES ("aardvarz");
EXPLAIN SELECT * FROM t1 ORDER BY word;
SELECT * FROM t1 ORDER BY word;
EXPLAIN SELECT word FROM t1 ORDER BY word;
SELECT word FROM t1 ORDER by word;
DROP TABLE t1;


#
# One field, index
#

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
SELECT * FROM t1 ORDER BY word;
DROP TABLE t1;

#
# Two fields, no index
#

CREATE TABLE t1 (
   word TEXT,
   bar INT(11) AUTO_INCREMENT,
   PRIMARY KEY (bar))
   ENGINE=MyISAM
   CHARSET utf32
   COLLATE utf32_general_ci ;
INSERT INTO t1 (word) VALUES ("aar");
INSERT INTO t1 (word) VALUES ("a" );
INSERT INTO t1 (word) VALUES ("aardvar");
INSERT INTO t1 (word) VALUES ("aardvark");
INSERT INTO t1 (word) VALUES ("aardvara");
INSERT INTO t1 (word) VALUES ("aardvarz");
EXPLAIN SELECT * FROM t1 ORDER BY word;
SELECT * FROM t1 ORDER BY word;
EXPLAIN SELECT word FROM t1 ORDER BY word;
SELECT word FROM t1 ORDER BY word;
DROP TABLE t1;

#
# END OF Bug 1264 test
#
########################################################

#
# Bug#9557 MyISAM utf8mb3 table crash
#
CREATE TABLE t1 (
  a varchar(250) NOT NULL default '',
  KEY a (a)
) ENGINE=MyISAM DEFAULT CHARSET=utf32 COLLATE utf32_general_ci;
insert into t1 values (0x803d);
insert into t1 values (0x005b);
--sorted_result
select hex(a) from t1;
drop table t1;

#
# Bug#22052 Trailing spaces are not removed from UNICODE fields in an index
#
create table t1 (
  a char(10) character set utf32 not null,
  index a (a)
) engine=myisam;
insert into t1 values (repeat(0x0000201f, 10));
insert into t1 values (repeat(0x00002020, 10));
insert into t1 values (repeat(0x00002021, 10));
# make sure "index read" is used
explain select hex(a) from t1 order by a;
select hex(a) from t1 order by a;
alter table t1 drop index a;
select hex(a) from t1 order by a;
drop table t1;

#
# Testing that maximum possible key length is 1000 bytes for MyISAM
#
create table t1 (a varchar(250) character set utf32 primary key) engine=MyISAM;
show create table t1;
drop table t1;
--error ER_TOO_LONG_KEY
create table t1 (a varchar(334) character set utf32 primary key) engine=MyISAM;

--echo #
--echo # Bug#42511 mysqld: ctype-ucs2.c:2044: my_strnncollsp_utf32: Assertion (tlen % 4) == 0' failed
--echo #
CREATE TABLE t1 (
 b char(250) CHARACTER SET utf32,
 key (b)
) ENGINE=MYISAM;
INSERT INTO t1 VALUES ('d'),('f');
SELECT * FROM t1 WHERE b BETWEEN 'a' AND 'z';
DROP TABLE t1;
