--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

--echo #
--echo # Start of 5.5 tests
--echo #

set names utf8mb3;
set collation_connection=utf32_unicode_ci;
select hex('a'), hex('a ');
-- source include/endspace.inc

#
# Bug #6787 LIKE not working properly with _ and utf8mb3 data
#
select 'c' like '\_' as want0; 

create table t1 (c1 char(10) character set utf32 collate utf32_bin);

--source include/ctype_unicode_latin.inc

select group_concat(c1 order by binary c1 separator '') from t1 group by c1 collate utf32_unicode_ci;
select group_concat(c1 order by binary c1 separator '') from t1 group by c1 collate utf32_icelandic_ci;
select group_concat(c1 order by binary c1 separator '') from t1 group by c1 collate utf32_latvian_ci;
select group_concat(c1 order by binary c1 separator '') from t1 group by c1 collate utf32_romanian_ci;
select group_concat(c1 order by binary c1 separator '') from t1 group by c1 collate utf32_slovenian_ci;
select group_concat(c1 order by binary c1 separator '') from t1 group by c1 collate utf32_polish_ci;
select group_concat(c1 order by binary c1 separator '') from t1 group by c1 collate utf32_estonian_ci;
select group_concat(c1 order by binary c1 separator '') from t1 group by c1 collate utf32_spanish_ci;
select group_concat(c1 order by binary c1 separator '') from t1 group by c1 collate utf32_swedish_ci;
select group_concat(c1 order by binary c1 separator '') from t1 group by c1 collate utf32_turkish_ci;
select group_concat(c1 order by binary c1 separator '') from t1 group by c1 collate utf32_czech_ci;
select group_concat(c1 order by binary c1 separator '') from t1 group by c1 collate utf32_danish_ci;
select group_concat(c1 order by binary c1 separator '') from t1 group by c1 collate utf32_lithuanian_ci;
select group_concat(c1 order by binary c1 separator '') from t1 group by c1 collate utf32_slovak_ci;
select group_concat(c1 order by binary c1 separator '') from t1 group by c1 collate utf32_spanish2_ci;
select group_concat(c1 order by binary c1 separator '') from t1 group by c1 collate utf32_roman_ci;
select group_concat(c1 order by binary c1 separator '') from t1 group by c1 collate utf32_esperanto_ci;
select group_concat(c1 order by binary c1 separator '') from t1 group by c1 collate utf32_hungarian_ci;
select group_concat(c1 order by binary c1 separator '') from t1 group by c1 collate utf32_croatian_ci;
select group_concat(c1 order by binary c1 separator '') from t1 group by c1 collate utf32_german2_ci;
select group_concat(c1 order by binary c1 separator '') from t1 group by c1 collate utf32_unicode_520_ci;
select group_concat(c1 order by binary c1 separator '') from t1 group by c1 collate utf32_vietnamese_ci;

drop table t1;

#
# Bug#5324
#
SET NAMES utf8mb3;
#test1
#Minimized the varchar length for InnoDB so that it does not throw max keylength exceeded error
CREATE TABLE t1 (c varchar(150) CHARACTER SET utf32 COLLATE utf32_general_ci NOT NULL, INDEX (c));
INSERT INTO t1 VALUES (_ucs2 0x039C03C903B403B11F770308);
#Check one row
SELECT * FROM t1 WHERE c LIKE _utf32 0x0000039C00000025 COLLATE utf32_general_ci;
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x039C03C903B4 USING utf8mb3));
#Check two rows
SELECT * FROM t1 WHERE c LIKE _utf32 0x0000039C00000025
COLLATE utf32_general_ci ORDER BY c;
DROP TABLE t1;
#test2
#Minimized the varchar length for InnoDB so that it does not throw max keylength exceeded error
CREATE TABLE t1 (c varchar(150) CHARACTER SET utf32 COLLATE utf32_unicode_ci NOT NULL, INDEX (c));
INSERT INTO t1 VALUES (_ucs2 0x039C03C903B403B11F770308);
#Check one row
SELECT * FROM t1 WHERE c LIKE _utf32 0x0000039C00000025 COLLATE utf32_unicode_ci;
INSERT INTO t1 VALUES (_ucs2 0x039C03C903B4);
#Check two rows
SELECT * FROM t1 WHERE c LIKE _utf32 0x0000039C00000025
COLLATE utf32_unicode_ci ORDER BY c;
DROP TABLE t1;
#test 3
#Minimized the varchar length for InnoDB so that it does not throw max keylength exceeded error
CREATE TABLE t1 (c varchar(150) CHARACTER SET utf32 COLLATE utf32_unicode_ci NOT NULL, INDEX (c));
INSERT INTO t1 VALUES (_ucs2 0x039C03C903B403B11F770308);
#Check one row row
SELECT * FROM t1 WHERE c LIKE CONVERT(_ucs2 0x039C0025 USING utf32) COLLATE utf32_unicode_ci;
INSERT INTO t1 VALUES (CONVERT(_ucs2 0x039C03C903B4 USING utf8mb3));
#Check two rows
SELECT * FROM t1 WHERE c LIKE CONVERT(_ucs2 0x039C0025 USING utf32)
COLLATE utf32_unicode_ci ORDER BY c;
DROP TABLE t1;


SET NAMES utf8mb3;
SET @test_character_set='utf32';
SET @test_collation='utf32_swedish_ci';
-- source include/ctype_common.inc


SET collation_connection='utf32_unicode_ci';
-- source include/ctype_filesort.inc
-- source include/ctype_like_escape.inc

--echo End of 4.1 tests

#
# Check UPPER/LOWER changing length
#
# Result shorter than argument
CREATE TABLE t1 (id int, a varchar(30) character set utf32);
INSERT INTO t1 VALUES (1, _ucs2 0x01310069), (2, _ucs2 0x01310131);
INSERT INTO t1 VALUES (3, _ucs2 0x00690069), (4, _ucs2 0x01300049);
INSERT INTO t1 VALUES (5, _ucs2 0x01300130), (6, _ucs2 0x00490049);
SELECT a, length(a) la, @l:=lower(a) l, length(@l) ll, @u:=upper(a) u, length(@u) lu
FROM t1 ORDER BY id;
ALTER TABLE t1 MODIFY a VARCHAR(30) character set utf32 collate utf32_turkish_ci;
SELECT a, length(a) la, @l:=lower(a) l, length(@l) ll, @u:=upper(a) u, length(@u) lu
FROM t1 ORDER BY id;
DROP TABLE t1;

#
# Test basic regex functionality
#
set collation_connection=utf32_unicode_ci;
--source include/ctype_regex.inc


#
# Test my_like_range and contractions
#
SET collation_connection=utf32_czech_ci;
--source include/ctype_czech.inc
--source include/ctype_like_ignorable.inc

--echo #
--echo # Bug #12319710 : INVALID MEMORY READ AND/OR CRASH IN 
--echo #   MY_UCA_CHARCMP WITH UTF32
--echo #

SET collation_connection=utf32_unicode_ci;
CREATE TABLE t1 (a TEXT CHARACTER SET utf32 COLLATE utf32_turkish_ci NOT NULL);
INSERT INTO t1 VALUES ('a'), ('b');
CREATE TABLE t2 (b VARBINARY(5) NOT NULL);

--echo #insert chars outside of BMP
INSERT INTO t2 VALUEs (0x082837),(0x082837);

--echo #test for read-out-of-bounds with non-BMP chars as a LIKE pattern
SELECT * FROM t1,t2 WHERE a LIKE b;

--echo #test the original statement
SELECT 1 FROM t1 AS t1_0 NATURAL LEFT OUTER JOIN t2 AS t2_0
RIGHT JOIN t1 AS t1_1 ON t1_0.a LIKE t2_0.b;

DROP TABLE t1,t2;

--echo #
--echo # End of 5.5 tests
--echo #


--echo #
--echo # Start of 5.6 tests
--echo #

--echo #
--echo # WL#3664 WEIGHT_STRING
--echo #

set collation_connection=utf32_unicode_ci;
--source include/weight_string.inc
--source include/weight_string_euro.inc
select hex(weight_string(_utf32 0x10000 collate utf32_unicode_ci));
select hex(weight_string(_utf32 0x10001 collate utf32_unicode_ci));

set @@collation_connection=utf32_czech_ci;
--source include/weight_string_chde.inc

#
# WL#4013 Unicode german2 collation
#
SET NAMES utf8mb3;
SET collation_connection=utf32_german2_ci;
--source include/ctype_german.inc

--echo #
--echo # WL#2673 Unicode Collation Algorithm new version
--echo #
SET NAMES utf8mb4;
SET collation_connection=utf32_unicode_520_ci;
--source include/ctype_unicode520.inc

--echo #
--echo # End of 5.6 tests
--echo #
