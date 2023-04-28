# This test takes long time, so only run it with the --big-test mtr-flag.
--source include/big_test.inc


SET @test_character_set= 'gb18030';
SET @test_collation= 'gb18030_chinese_ci';

SET NAMES utf8mb4;
SET collation_connection=gb18030_chinese_ci;

#
# Populate table t1 with all ucs2 codes [00..FF][00..FF]
#
CREATE TABLE t1 (b VARCHAR(2));
INSERT INTO t1 VALUES ('0'), ('1'), ('2'), ('3'), ('4'), ('5'), ('6'), ('7');
INSERT INTO t1 VALUES ('8'), ('9'), ('A'), ('B'), ('C'), ('D'), ('E'), ('F');

CREATE TEMPORARY TABLE head AS SELECT concat(b1.b, b2.b) AS head FROM t1 b1, t1 b2;
CREATE TEMPORARY TABLE tail AS SELECT concat(b1.b, b2.b) AS tail FROM t1 b1, t1 b2;
DROP TABLE t1;

CREATE TABLE t1 (code char(1)) CHARACTER SET UCS2 ENGINE=INNODB;
INSERT INTO t1 SELECT UNHEX(CONCAT(head, tail)) FROM head, tail ORDER BY
head, tail;

#
# Populate table t2 with all 4-bytes codes [01..02][00..FF][00..FF]
# Should populate to 0x10FFFF, but it would cost too much time
# until the test is timeout.
#
CREATE TABLE t2 (b VARCHAR(2));
INSERT INTO t2 VALUES ('0'), ('1'), ('2'), ('3'), ('4'), ('5'), ('6'), ('7');
INSERT INTO t2 VALUES ('8'), ('9'), ('A'), ('B'), ('C'), ('D'), ('E'), ('F');
CREATE TEMPORARY TABLE ch1 AS SELECT concat(b1.b, b2.b) AS ch FROM t2 b1, t2 b2;
CREATE TEMPORARY TABLE ch2 AS SELECT concat(b1.b, b2.b) AS ch FROM t2 b1, t2 b2;
CREATE TEMPORARY TABLE ch3 AS SELECT concat(b1.b, b2.b) AS ch FROM t2 b1, t2 b2;
DROP TABLE t2;

CREATE TABLE t2 (code char(1)) CHARACTER SET UTF32 ENGINE=INNODB;
INSERT INTO t2 SELECT UNHEX(CONCAT(ch1.ch, ch2.ch, ch3.ch)) FROM ch1, ch2, ch3
WHERE (ch1.ch BETWEEN '01' AND '02') AND (ch2.ch BETWEEN '00' AND 'FF')
AND (ch3.ch BETWEEN '00' AND 'FF')
ORDER BY ch1.ch, ch2.ch, ch3.ch;

DROP TEMPORARY TABLE head, tail;
DROP TEMPORARY TABLE ch1, ch2, ch3;

# Prevent 0x0020 being trimmed
SET sql_mode='PAD_CHAR_TO_FULL_LENGTH';

#####################################################################

--echo # Check [U+D800, U+DFFF] maps to ?(0x3F)

SELECT COUNT(*) FROM t1 WHERE CONVERT(code USING gb18030) <> 0x3F AND
code >= 0xD800 AND code <= 0xDFFF;

#####################################################################

--echo # Print all the upper/lower in gb18030

SELECT HEX(CONVERT(code USING gb18030)),
HEX(UPPER(CONVERT(code USING gb18030))),
HEX(LOWER(CONVERT(code USING gb18030))),
code,
UPPER(CONVERT(code USING gb18030)),
LOWER(CONVERT(code USING gb18030))
FROM t1
WHERE HEX(CONVERT(code USING gb18030)) <> HEX(UPPER(CONVERT(code USING gb18030)))
OR HEX(CONVERT(code USING gb18030)) <> HEX(LOWER(CONVERT(code USING gb18030)))
ORDER BY HEX(CONVERT(code USING gb18030));

SELECT HEX(CONVERT(code USING gb18030)),
HEX(UPPER(CONVERT(code USING gb18030))),
HEX(LOWER(CONVERT(code USING gb18030))),
code,
UPPER(CONVERT(code USING gb18030)),
LOWER(CONVERT(code USING gb18030))
FROM t2
WHERE HEX(CONVERT(code USING gb18030)) <> HEX(UPPER(CONVERT(code USING gb18030)))
OR HEX(CONVERT(code USING gb18030))<>HEX(LOWER(CONVERT(code USING gb18030)))
ORDER BY HEX(CONVERT(code USING gb18030));

#####################################################################

--echo # Convert ucs2 BMP->gb18030->utf8mb4
SELECT HEX(code), HEX(CONVERT(CONVERT(code USING gb18030) USING utf8mb4)) FROM t1;

--echo # Compare the results between ucs2 BMP->utf8mb4 and ucs2 BMP->gb18030->utf8mb4
--echo # GB18030 will treat code points in (0xD800, 0xE000] as 0x3F, this is the only difference
SELECT HEX(code) FROM t1 WHERE
(code < 0xD800 OR code >= 0xE000) AND HEX(CONVERT(code USING utf8mb4)) <> HEX(CONVERT(CONVERT(code USING gb18030) USING utf8mb4));

--echo # Test for differences between upper(ucs2_to_utf8mb4(_ucs2 x)) and gb18030_to_utf8mb4(upper(ucs2_to_gb18030(_ucs2 x)) for BMP

SELECT HEX(code), UPPER(CAST(code AS CHAR CHARACTER SET utf8mb4) COLLATE utf8mb4_unicode_520_ci), CONVERT(UPPER(CONVERT(code USING gb18030)) USING utf8mb4) FROM t1 WHERE
(code < 0xD800 OR code >= 0xE000) AND
UPPER(CAST(code AS CHAR CHARACTER SET utf8mb4) COLLATE utf8mb4_unicode_520_ci) <> CONVERT(UPPER(CONVERT(code USING gb18030)) USING utf8mb4);

--echo # Test for differences between lower(ucs2_to_utf8mb4(_ucs2 x)) and gb18030_to_utf8mb4(lower(ucs2_to_gb18030(_ucs2 x)) for BMP

SELECT HEX(code), LOWER(CAST(code AS CHAR CHARACTER SET utf8mb4) COLLATE utf8mb4_unicode_520_ci), CONVERT(UPPER(CONVERT(code USING gb18030)) USING utf8mb4) FROM t1 WHERE
(code < 0xD800 OR code >= 0xE000) AND
LOWER(CAST(code AS CHAR CHARACTER SET utf8mb4) COLLATE utf8mb4_unicode_520_ci) <> CONVERT(LOWER(CONVERT(code USING gb18030)) USING utf8mb4);

#####################################################################

SELECT COUNT(*) FROM t2;

#--echo # Convert utf32 [010000,10FFFF]->gb18030->utf8mb4
# The output of the query is too big to save into the result file
#SELECT HEX(code), HEX(CONVERT(CONVERT(code USING gb18030) USING utf8mb4)) FROM t2;

--echo # Compare the results between utf32->utf8mb4 and utf32->gb18030->utf8mb4
SELECT COUNT(*) FROM t2 WHERE
HEX(CONVERT(code USING utf8mb4)) <> HEX(CONVERT(CONVERT(code USING gb18030) USING utf8mb4));

SELECT HEX(code), UPPER(CAST(code AS CHAR CHARACTER SET utf8mb4) COLLATE utf8mb4_unicode_520_ci), CONVERT(UPPER(CONVERT(code USING gb18030)) USING utf8mb4) FROM t2 WHERE
HEX(UPPER(CAST(code AS CHAR CHARACTER SET utf8mb4) COLLATE utf8mb4_unicode_520_ci)) <> HEX(CONVERT(UPPER(CONVERT(code USING gb18030)) USING utf8mb4));

SELECT HEX(code), LOWER(CAST(code AS CHAR CHARACTER SET utf8mb4) COLLATE utf8mb4_unicode_520_ci), CONVERT(UPPER(CONVERT(code USING gb18030)) USING utf8mb4) FROM t2 WHERE
HEX(LOWER(CAST(code AS CHAR CHARACTER SET utf8mb4) COLLATE utf8mb4_unicode_520_ci)) <> HEX(CONVERT(LOWER(CONVERT(code USING gb18030)) USING utf8mb4));

###################################################################
--echo
--echo # Test the sortorder for PINYIN collation
--echo

CREATE TABLE t3(c CHAR(1) CHARACTER SET gb18030);
INSERT INTO t3 SELECT CAST(code AS CHAR CHARACTER SET gb18030) FROM t1 WHERE
code >= 0x2E80 AND code <= 0x9FC3;
INSERT INTO t3 SELECT CAST(code AS CHAR CHARACTER SET gb18030) FROM t2 WHERE
code >=0x20000  AND code <= 0x2B6F8;

# The range[GB+B0A2, GB+C981] covers all code points defined in PINYIN coll
# According to CLDR24, there are 41309 code points
SELECT COUNT(*) FROM t3 WHERE c <= 0xC981 AND c >= 0xB0A2 ORDER BY c;
SELECT HEX(CONVERT(c USING utf32)) FROM t3 WHERE c <= 0xC981 AND c >= 0xB0A2 ORDER BY c;

DROP TABLE t1, t2, t3;
