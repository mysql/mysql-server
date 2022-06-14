SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
create table t1 (i int) engine=myisam max_rows=100000000000;
Warnings:
Warning	4074	max_rows=100000000000 is outside the valid range [0,4294967295]. 4294967295 will be used.
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci MAX_ROWS=4294967295
alter table t1 max_rows=100;
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci MAX_ROWS=100
alter table t1 max_rows=100000000000;
Warnings:
Warning	4074	max_rows=100000000000 is outside the valid range [0,4294967295]. 4294967295 will be used.
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci MAX_ROWS=4294967295
drop table t1;

# --
# -- Bug#18834: ALTER TABLE ADD INDEX on table with two timestamp fields
# --

CREATE TABLE t3(c1 DATETIME NOT NULL) ENGINE=MYISAM;
INSERT INTO t3 VALUES (0);

SET sql_mode = TRADITIONAL;

ALTER TABLE t3 ADD INDEX(c1);
ERROR 22007: Incorrect datetime value: '0000-00-00 00:00:00' for column 'c1' at row 1

# -- Cleanup.
SET sql_mode = '';
DROP TABLE t3;
