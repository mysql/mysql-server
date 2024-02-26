--source include/no_valgrind_without_big.inc

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

SET NAMES utf8mb4 COLLATE utf8mb4_0900_as_ci;

SET @test_character_set= 'utf8mb4';
SET @test_collation= 'utf8mb4_0900_as_ci';
-- source include/ctype_common.inc

SET NAMES utf8mb4 COLLATE utf8mb4_0900_as_ci;
-- source include/ctype_filesort.inc
-- source include/ctype_innodb_like.inc
-- source include/ctype_like_escape.inc
-- source include/ctype_ascii_order.inc

# Test maximum buffer necessary for WEIGHT_STRING
SELECT HEX(WEIGHT_STRING('aA'));
SELECT HEX(WEIGHT_STRING(CAST(_utf32 x'337F' AS CHAR)));
SELECT HEX(WEIGHT_STRING(CAST(_utf32 x'FDFA' AS CHAR)));

# Test WEIGHT_STRING, LOWER, UPPER
--source include/weight_string.inc
--source include/weight_string_euro.inc
--source include/ctype_unicode800.inc

--source include/ctype_inet.inc

CREATE TABLE t1 (c VARCHAR(10) CHARACTER SET utf8mb4);
INSERT INTO t1 VALUES (_utf8mb4 0xF09090A7), (_utf8mb4 0xEA8B93), (_utf8mb4 0xC4BC), (_utf8mb4 0xC6AD), (_utf8mb4 0xF090918F), (_utf8mb4 0xEAAD8B);

--sorted_result
SELECT HEX(ANY_VALUE(c)), COUNT(c) FROM t1 GROUP BY c COLLATE utf8mb4_0900_as_ci;
DROP TABLE t1;

SET NAMES utf8mb4;
CREATE TABLE t1 (c1 CHAR(10) COLLATE utf8mb4_0900_as_ci);

--source include/ctype_unicode_latin.inc
--source include/ctype_unicode_latin_1.inc

SELECT c1, HEX(WEIGHT_STRING(c1)) FROM t1 ORDER BY c1, HEX(c1) COLLATE utf8mb4_0900_as_ci;

DROP TABLE t1;
