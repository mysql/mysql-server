SET @saved_innodb_strict_mode=@@SESSION.innodb_strict_mode;
CREATE TABLE t1 (id INT PRIMARY KEY, c1 VARCHAR(4000), c2 VARCHAR(4000), c3 VARCHAR(500)) ROW_FORMAT=COMPACT;
ERROR 42000: Row size too large (> 1982). Changing some columns to TEXT or BLOB or using ROW_FORMAT=DYNAMIC or ROW_FORMAT=COMPRESSED may help. In current row format, BLOB prefix of 768 bytes is stored inline.
CREATE TABLE t1 (id INT PRIMARY KEY, c1 VARCHAR(4000), c2 VARCHAR(4000), c3 VARCHAR(500)) ROW_FORMAT=REDUNDANT;
ERROR 42000: Row size too large (> 1979). Changing some columns to TEXT or BLOB or using ROW_FORMAT=DYNAMIC or ROW_FORMAT=COMPRESSED may help. In current row format, BLOB prefix of 768 bytes is stored inline.
SET SESSION innodb_strict_mode=OFF;
CREATE TABLE t1 (id INT PRIMARY KEY, c1 VARCHAR(4000), c2 VARCHAR(4000), c3 VARCHAR(500)) ROW_FORMAT=COMPACT;
Warnings:
Warning	139	Row size too large (> 1982). Changing some columns to TEXT or BLOB or using ROW_FORMAT=DYNAMIC or ROW_FORMAT=COMPRESSED may help. In current row format, BLOB prefix of 768 bytes is stored inline.
INSERT INTO t1 VALUES(2, repeat('e', 4000), repeat('f', 4000), repeat('h', 500));
ERROR 42000: Row size too large (> 1982). Changing some columns to TEXT or BLOB or using ROW_FORMAT=DYNAMIC or ROW_FORMAT=COMPRESSED may help. In current row format, BLOB prefix of 768 bytes is stored inline.
DROP TABLE t1;
CREATE TABLE t1 (id INT PRIMARY KEY, c1 VARCHAR(4000), c2 VARCHAR(4000), c3 VARCHAR(500)) ROW_FORMAT=REDUNDANT;
Warnings:
Warning	139	Row size too large (> 1979). Changing some columns to TEXT or BLOB or using ROW_FORMAT=DYNAMIC or ROW_FORMAT=COMPRESSED may help. In current row format, BLOB prefix of 768 bytes is stored inline.
INSERT INTO t1 VALUES(2, repeat('e', 4000), repeat('f', 4000), repeat('h', 500));
ERROR 42000: Row size too large (> 1979). Changing some columns to TEXT or BLOB or using ROW_FORMAT=DYNAMIC or ROW_FORMAT=COMPRESSED may help. In current row format, BLOB prefix of 768 bytes is stored inline.
DROP TABLE t1;
SET SESSION innodb_strict_mode=@saved_innodb_strict_mode;
CREATE TABLE t1 (id INT PRIMARY KEY, c1 VARCHAR(4000), c2 VARCHAR(4000), c3 VARCHAR(500)) ROW_FORMAT=DYNAMIC;
INSERT INTO t1 VALUES(2, repeat('e', 4000), repeat('f', 4000), repeat('h', 500));
SELECT id, LEFT(c1 , 10), LEFT(c2, 10), LEFT(c3, 10) FROM t1;
id	LEFT(c1 , 10)	LEFT(c2, 10)	LEFT(c3, 10)
2	eeeeeeeeee	ffffffffff	hhhhhhhhhh
DROP TABLE t1;
