--source include/force_myisam_default.inc
--source include/have_myisam.inc

--echo #
--echo # Bug#1264
--echo #
# Description:
#
# When USING a ucs2 TABLE in MySQL,
# either with ucs2_general_ci or ucs2_bin collation,
# words are returned in an incorrect order when USING ORDER BY
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

SET NAMES utf8mb3, collation_connection=utf16le_general_ci;

--echo #
--echo # Two fields, index
--echo #

CREATE TABLE t1 (
   word VARCHAR(64),
   bar INT(11) DEFAULT 0,
   PRIMARY KEY (word))
   ENGINE=MyISAM
   CHARSET utf16le
   COLLATE utf16le_general_ci ;

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


--echo #
--echo # One field, index
--echo #

CREATE TABLE t1 (
   word VARCHAR(64) ,
   PRIMARY KEY (word))
   ENGINE=MyISAM
   CHARSET utf16le
   COLLATE utf16le_general_ci;

INSERT INTO t1 (word) VALUES ("aar");
INSERT INTO t1 (word) VALUES ("a");
INSERT INTO t1 (word) VALUES ("aardvar");
INSERT INTO t1 (word) VALUES ("aardvark");
INSERT INTO t1 (word) VALUES ("aardvara");
INSERT INTO t1 (word) VALUES ("aardvarz");
EXPLAIN SELECT * FROM t1 ORDER BY WORD;
SELECT * FROM t1 ORDER BY word;
DROP TABLE t1;


--echo #
--echo # Two fields, no index
--echo #

CREATE TABLE t1 (
   word TEXT,
   bar INT(11) AUTO_INCREMENT,
   PRIMARY KEY (bar))
   ENGINE=MyISAM
   CHARSET utf16le
   COLLATE utf16le_general_ci ;
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

--echo #
--echo # END OF Bug 1264 test
--echo #


--echo #
--echo # Bug#22052 Trailing spaces are not removed FROM UNICODE fields in an index
--echo #
CREATE TABLE t1 (
  a CHAR(10) CHARACTER SET utf16le NOT NULL,
  INDEX a (a)
) engine=myisam;
INSERT INTO t1 VALUES (REPEAT(_ucs2 0x201f, 10));
INSERT INTO t1 VALUES (REPEAT(_ucs2 0x2020, 10));
INSERT INTO t1 VALUES (REPEAT(_ucs2 0x2021, 10));
--echo # make sure "index read" is used
explain SELECT HEX(a) FROM t1 ORDER BY a;
SELECT HEX(a) FROM t1 ORDER BY a;
ALTER TABLE t1 DROP INDEX a;
SELECT HEX(a) FROM t1 ORDER BY a;
DROP TABLE t1;

--echo #
--echo # Testing that maximim possible key length is 1000 bytes for MyISAM
--echo #
CREATE TABLE t1 (a VARCHAR(250) CHARACTER SET utf16le PRIMARY KEY) engine=MyISAM;
SHOW CREATE TABLE t1;
DROP TABLE t1;
--error ER_TOO_LONG_KEY
CREATE TABLE t1 (a VARCHAR(334) CHARACTER SET utf16le PRIMARY KEY) engine=MyISAM;
