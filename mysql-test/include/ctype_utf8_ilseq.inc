#
# Compare a field to an utf8 string literal with illegal byte sequences
#

--echo #
--echo # Start of ctype_utf8_ilseq.inc
--echo #

--eval CREATE TABLE t1 ENGINE=$ENGINE AS SELECT REPEAT(' ', 60) AS ch LIMIT 0;
ALTER TABLE t1
  ADD id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  ADD KEY(ch);
SHOW CREATE TABLE t1;

INSERT INTO t1 (ch) VALUES ('admin'),('admin1');
SELECT ch FROM t1 WHERE ch='admin𝌆';
SELECT ch FROM t1 IGNORE KEY (ch) WHERE ch='admin𝌆';
SELECT ch FROM t1 FORCE KEY (ch) WHERE ch='admin𝌆';
DELETE FROM t1;
INSERT INTO t1 (ch) VALUES ('a'), ('a?'), ('a??'), ('a???'), ('a????');
INSERT INTO t1 (ch) VALUES ('ab'),('a?b'),('a??b'),('a???b'),('a????b');
INSERT INTO t1 (ch) VALUES ('az'),('a?z'),('a??z'),('a???z'),('a????z');
INSERT INTO t1 (ch) VALUES ('z');
# LATIN SMALL LETTER A + LATIN CAPITAL LETTER E WITH GRAVE
INSERT INTO t1 (ch) VALUES (_utf8 0x61D080);
# LATIN SMALL LETTER A + ARMENIAN SMALL LETTER REH
INSERT INTO t1 (ch) VALUES (_utf8 0x61D680);

SELECT ch FROM t1 IGNORE KEY (ch) WHERE ch='a𝌆' ORDER BY ch;
SELECT ch FROM t1 IGNORE KEY (ch) WHERE ch='a𝌆b' ORDER BY ch;
SELECT ch FROM t1 FORCE KEY (ch) WHERE ch='a𝌆' ORDER BY ch;
SELECT ch FROM t1 FORCE KEY (ch) WHERE ch='a𝌆b' ORDER BY ch;

SELECT ch FROM t1 IGNORE KEY (ch) WHERE ch<'a𝌆' ORDER BY ch;
SELECT ch FROM t1 IGNORE KEY (ch) WHERE ch<'a𝌆b' ORDER BY ch;
SELECT ch FROM t1 FORCE KEY (ch) WHERE ch<'a𝌆' ORDER BY ch;
SELECT ch FROM t1 FORCE KEY (ch) WHERE ch<'a𝌆b' ORDER BY ch;

SELECT ch FROM t1 IGNORE KEY (ch) WHERE ch>'a𝌆' ORDER BY ch;
SELECT ch FROM t1 IGNORE KEY (ch) WHERE ch>'a𝌆b' ORDER BY ch;
SELECT ch FROM t1 FORCE KEY (ch) WHERE ch>'a𝌆' ORDER BY ch;
SELECT ch FROM t1 FORCE KEY (ch) WHERE ch>'a𝌆b' ORDER BY ch;

ALTER TABLE t1 DROP KEY ch;

--echo # 0xD18F would be a good 2-byte character, 0xD1 is an incomplete sequence
SET @query=CONCAT('SELECT ch FROM t1 WHERE ch=''a', 0xD1,'''');
PREPARE stmt FROM @query;
EXECUTE stmt;
SET @query=CONCAT('SELECT ch FROM t1 WHERE ch=''a', 0xD1,'b''');
PREPARE stmt FROM @query;
EXECUTE stmt;

#
# Non-equality comparison currently work differently depending on collation:
#
# - utf8_general_ci falls back to memcmp() on bad byte
# - utf8_unicode_ci treats bad bytes greater than any valid character
#
#  For example, these two characters:
#    _utf8 0xD080 (U+00C8 LATIN CAPITAL LETTER E WITH GRAVE)
#    _utf8 0xD680 (U+0580 ARMENIAN SMALL LETTER REH)
#
#  will give different results (depending on collation) when compared 
#  to an incomplete byte sequence 0xD1 (mb2head not followed by mb2tail).
#
# For utf8_general_ci the result depends on the valid side:
# - 0xD080 is smaller than 0xD1, because 0xD0 < 0xD1
# - 0xD680 is greater than 0xD1, because 0xD6 > 0xD1
#
# For utf8_unicode_ci the result does not depend on the valid side:
# - 0xD080 is smaller than 0xD1, because 0xD1 is greater than any valid character
# - 0xD680 is smaller than 0xD1, because 0xD1 is greater than any valid character
#
# utf8_general_ci should be eventually fixed to treat bad bytes greater
# than any valid character, similar to utf8_unicode_ci.
#

SET @query=CONCAT('SELECT ch FROM t1 WHERE ch<''a', 0xD1,''' ORDER BY ch');
PREPARE stmt FROM @query;
EXECUTE stmt;
SET @query=CONCAT('SELECT ch FROM t1 WHERE ch>''a', 0xD1,''' ORDER BY ch');
PREPARE stmt FROM @query;
EXECUTE stmt;

--echo # 0xEA9A96 would be a good 3-byte character, 0xEA9A is an incomplete sequence
SET @query=CONCAT('SELECT ch FROM t1 WHERE ch=''a', 0xEA9A,''' ORDER BY ch');
PREPARE stmt FROM @query;
EXECUTE stmt;
SET @query=CONCAT('SELECT ch FROM t1 WHERE ch=''a', 0xEA9A,'b'' ORDER BY ch');
PREPARE stmt FROM @query;
EXECUTE stmt;

--echo # 0x8F is a bad byte sequence (an mb2tail without mb2head)
SET @query=CONCAT('SELECT ch FROM t1 WHERE ch=''a', 0x8F,''' ORDER BY ch');
PREPARE stmt FROM @query;
EXECUTE stmt;
SET @query=CONCAT('SELECT ch FROM t1 WHERE ch=''a', 0x8F,'b'' ORDER BY ch');
PREPARE stmt FROM @query;
EXECUTE stmt;

--echo # 0x8F8F is a bad byte sequence (an mb2tail without mb2head, two times)
SET @query=CONCAT('SELECT ch FROM t1 WHERE ch=''a', 0x8F8F,''' ORDER BY ch');
PREPARE stmt FROM @query;
EXECUTE stmt;
SET @query=CONCAT('SELECT ch FROM t1 WHERE ch=''a', 0x8F8F,'b'' ORDER BY ch');
PREPARE stmt FROM @query;
EXECUTE stmt;

DROP TABLE t1;

--echo #
--echo # End of ctype_utf8_ilseq.inc
--echo #
