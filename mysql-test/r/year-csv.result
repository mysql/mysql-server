SET global default_storage_engine=CSV;
SET session default_storage_engine=CSV;
# Test for warnings on column creation
CREATE TABLE t1 (y YEAR NOT NULL);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `y` year NOT NULL
) ENGINE=CSV DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t1;
CREATE TABLE t1 (y YEAR(-4) NOT NULL);
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '-4) NOT NULL)' at line 1
CREATE TABLE t1 (y YEAR(0) NOT NULL);
ERROR HY000: Invalid display width. Use YEAR instead.
CREATE TABLE t1 (y YEAR(1) NOT NULL);
ERROR HY000: Invalid display width. Use YEAR instead.
CREATE TABLE t1 (y YEAR(2) NOT NULL);
ERROR HY000: Invalid display width. Use YEAR instead.
CREATE TABLE t1 (y YEAR(5) NOT NULL);
ERROR HY000: Invalid display width. Use YEAR instead.
CREATE TABLE t1 (y YEAR(10240) NOT NULL);
ERROR HY000: Invalid display width. Use YEAR instead.
CREATE TABLE t1 (y YEAR(-1) NOT NULL);
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '-1) NOT NULL)' at line 1
CREATE TABLE t1 (y YEAR(-4294967296) NOT NULL);
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '-4294967296) NOT NULL)' at line 1
CREATE TABLE t1 (y YEAR(-4294967295) NOT NULL);
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '-4294967295) NOT NULL)' at line 1
CREATE TABLE t1 (y YEAR(NULL) NOT NULL);
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'NULL) NOT NULL)' at line 1
CREATE TABLE t1 (y YEAR('') NOT NULL);
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ''') NOT NULL)' at line 1
CREATE TABLE t1 (y YEAR('a') NOT NULL);
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ''a') NOT NULL)' at line 1
CREATE TABLE t1 (y YEAR('-a') NOT NULL);
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ''-a') NOT NULL)' at line 1
CREATE TABLE t1 (y YEAR(4294967296) NOT NULL);
ERROR HY000: Invalid display width. Use YEAR instead.
CREATE TABLE t1 (y YEAR(4294967295) NOT NULL);
ERROR HY000: Invalid display width. Use YEAR instead.
# Test for default column width value
CREATE TABLE t1 (y YEAR NOT NULL);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `y` year NOT NULL
) ENGINE=CSV DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t1;
#
# Check various ALTER TABLE operations on YEAR(2) type
#
CREATE TABLE t1 (i INT NOT NULL);
# Try to create new YEAR(2) column with ALTER TABLE
ALTER TABLE t1 ADD COLUMN y YEAR(2) NOT NULL;
ERROR HY000: Invalid display width. Use YEAR instead.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int NOT NULL
) ENGINE=CSV DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
# Try to convert YEAR(4) column to YEAR(2) with ALTER TABLE
ALTER TABLE t1 MODIFY COLUMN y YEAR(2) NOT NULL;
ERROR HY000: Invalid display width. Use YEAR instead.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int NOT NULL
) ENGINE=CSV DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ALTER TABLE t1 MODIFY COLUMN i YEAR(2) NOT NULL;
ERROR HY000: Invalid display width. Use YEAR instead.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int NOT NULL
) ENGINE=CSV DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ALTER TABLE t1 MODIFY COLUMN i INT NOT NULL;
DROP TABLE t1;
#
# Regression tests
#
CREATE TABLE t1 (y YEAR NOT NULL DEFAULT 0,
i INT NOT NULL DEFAULT 0,
c VARCHAR(20) NOT NULL DEFAULT '');
# insert [1901..2155]
# insert [0..99]
UPDATE t1 SET i = c;
# should return no warnings
UPDATE t1 SET y = c;
# should return no warnings
UPDATE t1 SET y = i;
# should return no warnings
INSERT INTO t1 (y) VALUES (0), ('00'), ('0000');
TRUNCATE t1;
# test out-of-range values, should return warnings
INSERT INTO t1 (i) VALUES (-1), (1800), (2156), (2147483647);
# should return 4 warnings
UPDATE IGNORE t1 SET y = i;
Warnings:
Warning	1264	Out of range value for column 'y' at row 1
Warning	1264	Out of range value for column 'y' at row 2
Warning	1264	Out of range value for column 'y' at row 3
Warning	1264	Out of range value for column 'y' at row 4
SELECT y, i FROM t1;
y	i
0000	-1
0000	1800
0000	2147483647
0000	2156
TRUNCATE t1;
# should return 4 warnings
INSERT INTO t1 (c) VALUES (''), (' '), ('2012qwer'), ('qwer');
UPDATE IGNORE t1 SET y = c;
Warnings:
Warning	1366	Incorrect integer value: '' for column 'y' at row 1
Warning	1366	Incorrect integer value: ' ' for column 'y' at row 2
Warning	1265	Data truncated for column 'y' at row 3
Warning	1366	Incorrect integer value: 'qwer' for column 'y' at row 4
SELECT y, CONCAT('"', c, '"') FROM t1;
y	CONCAT('"', c, '"')
0000	" "
0000	""
0000	"qwer"
2012	"2012qwer"
DROP TABLE t1;
SET @@global.default_storage_engine = InnoDB;
SET @@session.default_storage_engine = InnoDB;
# 
