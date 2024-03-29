--source include/no_valgrind_without_big.inc

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

--echo #
--echo # Start of 8.0 tests
--echo #

--echo #
--echo # WL#9125: Add utf8mb4_0900_ai_ci
--echo #

SET NAMES utf8mb4 COLLATE utf8mb4_0900_ai_ci;

SET @test_character_set= 'utf8mb4';
SET @test_collation= 'utf8mb4_0900_ai_ci';
-- source include/ctype_common.inc

SET NAMES utf8mb4 COLLATE utf8mb4_0900_ai_ci;
-- source include/ctype_filesort.inc
-- source include/ctype_innodb_like.inc
-- source include/ctype_like_escape.inc
-- source include/ctype_ascii_order.inc

# Test some conversions
SET NAMES utf8mb4 COLLATE utf8mb4_0900_ai_ci;
SELECT HEX(CONVERT(_utf8mb4 0xF091AB9B41 USING ucs2));
SELECT HEX(CONVERT(_utf8mb4 0xF091AB9B41 USING utf16));
SELECT HEX(CONVERT(_utf8mb4 0xF091AB9B41 USING utf32));
SELECT HEX(CONVERT(_ucs2 0xF8FF USING utf8mb4));
SELECT HEX(CONVERT(_utf16 0xF8FF USING utf8mb4));
SELECT HEX(CONVERT(_utf32 0xF8FF USING utf8mb4));
--ERROR ER_INVALID_CHARACTER_STRING
SELECT HEX(CONVERT(_utf8mb4 0x8F USING ucs2));
--ERROR ER_INVALID_CHARACTER_STRING
SELECT HEX(CONVERT(_utf8mb4 0xC230 USING ucs2));
--ERROR ER_INVALID_CHARACTER_STRING
SELECT HEX(CONVERT(_utf8mb4 0xE234F1 USING ucs2));
--ERROR ER_INVALID_CHARACTER_STRING
SELECT HEX(CONVERT(_utf8mb4 0xF4E25634 USING ucs2));

# Test some string functions
SELECT ASCII('ABC');
SELECT BIT_LENGTH('a');
SELECT BIT_LENGTH('À');
SELECT BIT_LENGTH('テ');
SELECT BIT_LENGTH('𝌆');
SELECT CHAR_LENGTH('𝌆テÀa');
SELECT LENGTH('𝌆テÀa');
SELECT FIELD('a', '𝌆テÀa');
SELECT HEX('𝌆テÀa');
SELECT INSERT('𝌆テÀa', 2, 2, 'テb');
SELECT LOWER('𝌆テÀBcd');
SELECT ORD('𝌆');
SELECT UPPER('𝌆テàbCD');
SELECT LOCATE(_utf8mb4 0xF091AB9B41, _utf8mb4 0xF091AB9B42F091AB9B41F091AB9B43);
SELECT HEX(REVERSE(_utf8mb4 0xF091AB9B41F091AB9B42F091AB9B43));
SELECT HEX(SUBSTRING(_utf8mb4 0xF091AB9B41F091AB9B42F091AB9B43, 1, 2));
SELECT HEX(SUBSTRING(_utf8mb4 0xF091AB9B41F091AB9B42F091AB9B43, -3, 2));
SELECT HEX(TRIM(_utf8mb4 0x2020F091AB9B4120F091AB9B4120202020));


# Test maximum buffer necessary for WEIGHT_STRING
SELECT HEX(WEIGHT_STRING('aA'));
SELECT HEX(WEIGHT_STRING(CAST(_utf32 x'337F' AS CHAR)));
SELECT HEX(WEIGHT_STRING(CAST(_utf32 x'FDFA' AS CHAR)));

# Test WEIGHT_STRING, LOWER, UPPER
--source include/weight_string.inc
--source include/weight_string_euro.inc
--source include/ctype_unicode800.inc

SELECT 'a' = 'a ';
SELECT 'a\0' < 'a';
SELECT 'a\0' < 'a ';
SELECT 'a\t' < 'a';
SELECT 'a\t' < 'a ';

SELECT 'a' LIKE 'a';
SELECT 'A' LIKE 'a';
SELECT _utf8mb4 0xD0B0D0B1D0B2 LIKE CONCAT(_utf8mb4'%',_utf8mb4 0xD0B1,_utf8mb4 '%');

--source include/ctype_inet.inc

CREATE TABLE t1 (c VARCHAR(10) CHARACTER SET utf8mb4);
INSERT INTO t1 VALUES (_utf8mb4 0xF09090A7), (_utf8mb4 0xEA8B93), (_utf8mb4 0xC4BC), (_utf8mb4 0xC6AD), (_utf8mb4 0xF090918F), (_utf8mb4 0xEAAD8B);

--sorted_result
SELECT HEX(ANY_VALUE(c)), COUNT(c) FROM t1 GROUP BY c COLLATE utf8mb4_0900_ai_ci;
DROP TABLE t1;

CREATE TABLE t1 (a VARCHAR(10), b VARCHAR(10)) COLLATE utf8mb4_0900_ai_ci;
INSERT INTO t1 VALUES(_utf16 0xAC00, _utf16 0x326E), (_utf16 0xAD, _utf16 0xA0),
(_utf16 0xC6, _utf16 0x41), (_utf16 0xC6, _utf16 0xAA), (_utf16 0xA73A, _utf16 0xA738);

SELECT a = b FROM t1;
DROP TABLE t1;

SET NAMES utf8mb4;
CREATE TABLE t1 (c1 CHAR(10) CHARACTER SET utf8mb4);

--source include/ctype_unicode_latin.inc
--source include/ctype_unicode_latin_1.inc

SELECT GROUP_CONCAT(HEX(CONVERT(c1 USING utf16)) ORDER BY c1, HEX(c1) SEPARATOR ' ') FROM t1 GROUP BY c1 COLLATE utf8mb4_0900_ai_ci;
SELECT GROUP_CONCAT(HEX(CONVERT(c1 USING utf16)) ORDER BY c1, HEX(c1) SEPARATOR ' ') FROM t1 GROUP BY c1 COLLATE utf8mb4_de_pb_0900_ai_ci;
SELECT GROUP_CONCAT(HEX(CONVERT(c1 USING utf16)) ORDER BY c1, HEX(c1) SEPARATOR ' ') FROM t1 GROUP BY c1 COLLATE utf8mb4_is_0900_ai_ci;
SELECT GROUP_CONCAT(HEX(CONVERT(c1 USING utf16)) ORDER BY c1, HEX(c1) SEPARATOR ' ') FROM t1 GROUP BY c1 COLLATE utf8mb4_lv_0900_ai_ci;
SELECT GROUP_CONCAT(HEX(CONVERT(c1 USING utf16)) ORDER BY c1, HEX(c1) SEPARATOR ' ') FROM t1 GROUP BY c1 COLLATE utf8mb4_ro_0900_ai_ci;
SELECT GROUP_CONCAT(HEX(CONVERT(c1 USING utf16)) ORDER BY c1, HEX(c1) SEPARATOR ' ') FROM t1 GROUP BY c1 COLLATE utf8mb4_sl_0900_ai_ci;
SELECT GROUP_CONCAT(HEX(CONVERT(c1 USING utf16)) ORDER BY c1, HEX(c1) SEPARATOR ' ') FROM t1 GROUP BY c1 COLLATE utf8mb4_pl_0900_ai_ci;
SELECT GROUP_CONCAT(HEX(CONVERT(c1 USING utf16)) ORDER BY c1, HEX(c1) SEPARATOR ' ') FROM t1 GROUP BY c1 COLLATE utf8mb4_et_0900_ai_ci;
SELECT GROUP_CONCAT(HEX(CONVERT(c1 USING utf16)) ORDER BY c1, HEX(c1) SEPARATOR ' ') FROM t1 GROUP BY c1 COLLATE utf8mb4_es_0900_ai_ci;
SELECT GROUP_CONCAT(HEX(CONVERT(c1 USING utf16)) ORDER BY c1, HEX(c1) SEPARATOR ' ') FROM t1 GROUP BY c1 COLLATE utf8mb4_sv_0900_ai_ci;
SELECT GROUP_CONCAT(HEX(CONVERT(c1 USING utf16)) ORDER BY c1, HEX(c1) SEPARATOR ' ') FROM t1 GROUP BY c1 COLLATE utf8mb4_tr_0900_ai_ci;
SELECT GROUP_CONCAT(HEX(CONVERT(c1 USING utf16)) ORDER BY c1, HEX(c1) SEPARATOR ' ') FROM t1 GROUP BY c1 COLLATE utf8mb4_cs_0900_ai_ci;
SELECT GROUP_CONCAT(HEX(CONVERT(c1 USING utf16)) ORDER BY c1, HEX(c1) SEPARATOR ' ') FROM t1 GROUP BY c1 COLLATE utf8mb4_da_0900_ai_ci;
SELECT GROUP_CONCAT(HEX(CONVERT(c1 USING utf16)) ORDER BY c1, HEX(c1) SEPARATOR ' ') FROM t1 GROUP BY c1 COLLATE utf8mb4_lt_0900_ai_ci;
SELECT GROUP_CONCAT(HEX(CONVERT(c1 USING utf16)) ORDER BY c1, HEX(c1) SEPARATOR ' ') FROM t1 GROUP BY c1 COLLATE utf8mb4_sk_0900_ai_ci;
SELECT GROUP_CONCAT(HEX(CONVERT(c1 USING utf16)) ORDER BY c1, HEX(c1) SEPARATOR ' ') FROM t1 GROUP BY c1 COLLATE utf8mb4_es_trad_0900_ai_ci;
SELECT GROUP_CONCAT(HEX(CONVERT(c1 USING utf16)) ORDER BY c1, HEX(c1) SEPARATOR ' ') FROM t1 GROUP BY c1 COLLATE utf8mb4_la_0900_ai_ci;
SELECT GROUP_CONCAT(HEX(CONVERT(c1 USING utf16)) ORDER BY c1, HEX(c1) SEPARATOR ' ') FROM t1 GROUP BY c1 COLLATE utf8mb4_eo_0900_ai_ci;
SELECT GROUP_CONCAT(HEX(CONVERT(c1 USING utf16)) ORDER BY c1, HEX(c1) SEPARATOR ' ') FROM t1 GROUP BY c1 COLLATE utf8mb4_hu_0900_ai_ci;
SELECT GROUP_CONCAT(HEX(CONVERT(c1 USING utf16)) ORDER BY c1, HEX(c1) SEPARATOR ' ') FROM t1 GROUP BY c1 COLLATE utf8mb4_hr_0900_ai_ci;
SELECT GROUP_CONCAT(HEX(CONVERT(c1 USING utf16)) ORDER BY c1, HEX(c1) SEPARATOR ' ') FROM t1 GROUP BY c1 COLLATE utf8mb4_vi_0900_ai_ci;

DROP TABLE t1;

CREATE TABLE t1 (a VARCHAR(1)) COLLATE utf8mb4_da_0900_ai_ci;
# Test characters have only 1 weight
INSERT INTO t1 VALUES('a'), ('b'), ('c'), ('d'), ('e');
SELECT HEX(a), HEX(WEIGHT_STRING(a)) FROM t1 ORDER BY a;

TRUNCATE TABLE t1;
# Test characters have more than 1 weight
INSERT INTO t1 VALUES(_utf16 0x00C4), (_utf16 0x00C5), (_utf16 0x00C6), (_utf16 0x00D8), (_utf16 0x00D6);
SELECT HEX(a), HEX(WEIGHT_STRING(a)) FROM t1 ORDER BY a, BINARY a;
DROP TABLE t1;

CREATE TABLE t1 (a VARCHAR(10), b VARCHAR(10)) COLLATE utf8mb4_da_0900_ai_ci;
INSERT INTO t1 VALUES(_utf16 0x015A, _utf16 0x00DF), (_utf16 0x0162, _utf16 0x00DE), (_utf16 0x01CF, _utf16 0x0132), (_utf16 0x01F8, _utf16 0x01CA), (_utf16 0x42, _utf16 0x1d2d);
SELECT HEX(CONVERT(a USING UTF16)) AS U16_a, HEX(CONVERT(b USING UTF16)) AS U16_b, a<b FROM t1;
DROP TABLE t1;

CREATE TABLE t1(a VARCHAR(10), b VARCHAR(10)) COLLATE utf8mb4_hu_0900_ai_ci;
INSERT INTO t1 VALUES(_utf16 0x01c4, _utf16 0x01f1), (_utf16 0x01f1, _utf16 0x02a4), ('cukor', 'csak');
SELECT HEX(CONVERT(a USING UTF16)) AS U16_a, HEX(CONVERT(b USING UTF16)) AS U16_b, a<b FROM t1;
DROP TABLE t1;

CREATE TABLE t1(a VARCHAR(10)) COLLATE utf8mb4_et_0900_ai_ci;
INSERT INTO t1 VALUES(_utf16 0x4F), (_utf16 0xD5), (_utf16 0x01A0), (_utf16 0x1EE0);
SELECT HEX(a), HEX(WEIGHT_STRING(a)) FROM t1 ORDER BY a, BINARY a;
DROP TABLE t1;

CREATE TABLE t1(a VARCHAR(10), b VARCHAR(10)) COLLATE utf8mb4_et_0900_ai_ci;
INSERT INTO t1 VALUES(_utf16 0x4F, _utf16 0x1EE0), (_utf16 0x1EE0, _utf16 0x1EE7), (_utf16 0x55, _utf16 0x1E7A), (_utf16 0x1E7A, _utf16 0x1E7C), (_utf16 0x1E7A, _utf16 0x1E80), (_utf16 0x4F, _utf16 0x1ED6), (_utf16 0x1ED6, _utf16 0x1EE4);
SELECT HEX(CONVERT(a USING UTF16)) AS U16_a, HEX(CONVERT(b USING UTF16)) AS U16_b, a<b FROM t1;
DROP TABLE t1;

CREATE TABLE t1(a VARCHAR(10), b VARCHAR(10)) COLLATE utf8mb4_cs_0900_ai_ci;
INSERT INTO t1 VALUES(_utf16 0x53, _utf16 0x1E66), (_utf16 0x53, _utf16 0x1E67), (_utf16 0x1E66, _utf16 0x1E9E);
SELECT HEX(CONVERT(a USING UTF16)) AS U16_a, HEX(CONVERT(b USING UTF16)) AS U16_b, a<b FROM t1;
DROP TABLE t1;

# Test reordering
CREATE TABLE t1(grp int, ch VARCHAR(10)) COLLATE utf8mb4_hr_0900_ai_ci;
# Following are space, currency, symbol characters
INSERT INTO t1 VALUES(0, '\t'), (0, ' '), (0, _utf16 0x5F), (0, _utf16 0x02DC), (0, '$'), (0, _utf16 0x20A0), (0, _utf16 0x2180);
# Following are Latin characters
INSERT INTO t1 VALUES(1, 'a'), (1, _utf16 0xA723), (1, _utf16 0x02AD);
# Following are Greek characters
INSERT INTO t1 VALUES(2, _utf16 0x03B1), (2, _utf16 0x1FA9), (2, _utf16 0x03F7);
# Following are Cyrillic characters
INSERT INTO t1 VALUES(3, _utf16 0x0430), (3, _utf16 0x046A), (3, _utf16 0x04C0);
# Following are other characters
INSERT INTO t1 VALUES(4, _utf16 0x2C30), (4, _utf16 0x10D0), (4, _utf16 0x0561), (4, _utf16 0x3041);
SELECT grp, HEX(CONVERT(ch USING utf16)) FROM t1 ORDER BY ch COLLATE utf8mb4_0900_ai_ci;
SELECT grp, HEX(CONVERT(ch USING utf16)) FROM t1 ORDER BY ch COLLATE utf8mb4_hr_0900_ai_ci;
DROP TABLE t1;

--echo #
--echo # Bug #25090543: DIFFERENCES BETWEEN ICU AND MYSQL FOR HUNGARIAN
--echo #                CONTRACTIONS/LIGATURES
--echo #
CREATE TABLE t1 (v VARCHAR(20) COLLATE utf8mb4_hu_0900_ai_ci);
INSERT INTO t1 VALUES ('dd'), ('a'), (_utf16 0x00E5), ('e'), ('d'), ('dz'),
       ('dzs'), ('ddz'), ('ddzs'), (_utf16 0x01F3), ('dza'), ('dzsa'),
       ('ddza'), ('ddzsa'), (_utf16 0x01F30061), ('dzu'), ('dzsu'),
       ('ddzu'), ('ddzsu'), (_utf16 0x01F30075), ('da'), ('dx'), ('dy'),
       (_utf16 0x006400E5), (_utf16 0x00640394);
SELECT v, HEX(CONVERT(v USING utf16)) AS HEX, HEX(WEIGHT_STRING(v)) FROM t1 ORDER BY v, HEX(v);
DROP TABLE t1;

CREATE TABLE t1 (a VARCHAR(10)) COLLATE utf8mb4_ru_0900_ai_ci;
INSERT INTO t1 VALUES(_utf16 0x0452), (_utf16 0x0453), (_utf16 0x0403),
       (_utf16 0x0439), (_utf16 0x048B), (_utf16 0x048A), (_utf16 0x043B),
       (_utf16 0x1D2B), (_utf16 0x045B), (_utf16 0x045C), (_utf16 0x040C);
SELECT HEX(CONVERT(a USING utf16)) AS codepoint FROM t1 ORDER BY a, HEX(a);
DROP TABLE t1;

# Mongolian langauage using Cryllic.
CREATE TABLE t1 (
      codepoint CHAR(1) CHARSET utf16 NOT NULL,
      glyph CHAR(2) CHARSET utf8mb4 COLLATE utf8mb4_mn_cyrl_0900_ai_ci NOT NULL,
      description VARCHAR(64) NOT NULL);
INSERT INTO t1 (codepoint, glyph, description) VALUES
      (0x041E, 'О', 'CYRILLIC CAPITAL LETTER O'),
      (0x04E8, 'Ө', 'CYRILLIC CAPITAL LETTER BARRED O'),
      (0x041F, 'П', 'CYRILLIC CAPITAL LETTER PE '),
      (0x043E, 'о', 'CYRILLIC SMALL LETTER O'),
      (0x04E9, 'ө', 'CYRILLIC SMALL LETTER BARRED O'),
      (0x043F, 'п', 'CYRILLIC SMALL LETTER PE'),
      (0x0423, 'У', 'CYRILLIC CAPITAL LETTER U '),
      (0x04AE, 'Ү', 'CYRILLIC CAPITAL LETTER STRAIGHT U '),
      (0x0424, 'Ф', 'CYRILLIC CAPITAL LETTER EF '),
      (0x0443, 'у', 'CYRILLIC SMALL LETTER U '),
      (0x04AF, 'ү', 'CYRILLIC SMALL LETTER STRAIGHT U'),
      (0x0444, 'ф', 'CYRILLIC SMALL LETTER EF');
 
SELECT HEX(codepoint), codepoint, glyph, description FROM t1 ORDER BY glyph, codepoint;
DROP TABLE t1;

--echo #
--echo # End of 8.0 tests
--echo #
