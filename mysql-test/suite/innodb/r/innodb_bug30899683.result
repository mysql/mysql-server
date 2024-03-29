set global table_definition_cache=400;
set global table_open_cache= 1024;
use test;
SET SESSION innodb_strict_mode = OFF;
CREATE TABLE t1(c1 INT PRIMARY KEY) COMPRESSION="zlibX";
Warnings:
Warning	1112	InnoDB: Unsupported compression algorithm 'zlibX'
SHOW WARNINGS;
Level	Code	Message
Warning	1112	InnoDB: Unsupported compression algorithm 'zlibX'
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `c1` int NOT NULL,
  PRIMARY KEY (`c1`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMPRESSION='zlibX'
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME = 't1';
TABLE_NAME	CREATE_OPTIONS
t1	COMPRESSION="zlibX"
SELECT NAME, ROW_FORMAT, SPACE_TYPE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME = 'test/t1';
NAME	ROW_FORMAT	SPACE_TYPE
test/t1	Dynamic	Single
INSERT INTO t1 VALUES(1);
commit;
use test;
SELECT * FROM t1;
c1
1
select sleep(10);
sleep(10)
0
# Flush tables, so that the table can be evicted by master thread.
flush tables;
# Create 1000 tables, try the best to evict the table.
sleep(20)
0
Warnings:
Warning	1525	Incorrect COMPRESSION value: 'zlibX'
Level	Code	Message
Warning	1525	Incorrect COMPRESSION value: 'zlibX'
set global table_definition_cache=default;
set global table_open_cache= default;
