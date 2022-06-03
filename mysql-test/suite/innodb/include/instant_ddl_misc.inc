##########################################################################
# Test script to test miscellaneous for table having INSTANT
# ADD/DROP columns.
#
# This test is to be run in debug mode only and with debug flag
##########################################################################

--source include/have_debug.inc
--source include/not_valgrind.inc
let $is_debug = 1;

SET SESSION debug= '+d,show_dropped_column';

--echo # Scenario 1
create table t1 (c1 char(10), c2 char(10));
--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc
insert into t1 values ("r1c1", "r1c2");
select * from t1;
alter table t1 add column c3 char(10) default "def_c3" after c1;
--source suite/innodb/include/print_instant_metadata.inc
insert into t1 values ("r2c1", "r2c3", "r2c2");
select * from t1;
drop table t1;

--echo # Scenario 2
create table t1 (c1 char(10), c2 char(10), c3 char(10));
insert into t1 values ("r1c1", "r1c2", "r1c3");
alter table t1 drop column c2;
--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc
select * from t1;
drop table t1;

--echo # Scenario 3
create table t1 (c1 char(10), c2 char(10));
insert into t1 values ("r1c1", "r1c2");
select * from t1;
alter table t1 add column c3 char(10) default "def_c3" after c1, algorithm=INSTANT;
insert into t1 values ("r2c1", "r2c3", "r2c2");
select * from t1;
alter table t1 drop column c2, algorithm=INSTANT;
select * from t1;
drop table t1;

--echo # Scenario 4
create table t1 (c1 char(10), c2 char(10), c3 char(10));
insert into t1 values ("r1c1", "r1c2", "r1c3");
select * from t1;
alter table t1 drop column c2;
select * from t1;
drop table t1;

--echo # Scenario 5
create table t1 (c1 char(10), c2 char(10));
insert into t1 values ("r1c1", "r1c2");
alter table t1 add column c3 char(10) default "def_c3" after c1;
select * from t1;
insert into t1 value ("r2c1", "r2c3", "r2c2");
alter table t1 drop column c2;
select * from t1;
drop table t1;

--echo # Scenario 6
create table t1 (c1 char(10), c2 char(10));
insert into t1 values ("r1c1", "r1c2");
alter table t1 add column c2 char(10) default "def_c3", drop column c2;
select * from t1;
insert into t1 value ("r2c1", "r2c2");
select * from t1;
drop table t1;

--echo # Scenario 7
create table t1 (c1 char(10), c2 char(10));
insert into t1 values ("r1c1", "r1c2");
alter table t1 add column c3 int after c1;
select * from t1;
insert into t1 value ("r2c1", 3, "r2c2");
select * from t1;
alter table t1 drop column c2;
select * from t1;
drop table t1;

--echo # Scenario 8
create table t1 (c1 char(10), c2 char(10), c3 char(10));
insert into t1 values ("r1c1", NULL, "r1c3");
select * from t1;
alter table t1 drop column c2;
--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc
select * from t1;
drop table t1;

--echo # Scenario 9
--eval CREATE TABLE t1 (C1 char(10), C2 char(10), C3 char(10)) ROW_FORMAT=$row_format
INSERT INTO t1 values ("r1c1", "r1c2", "r1c3");
alter table t1 drop column c1;
select * from t1;
alter table t1 drop column c3;
select * from t1;
drop table t1;

--echo # Scenario 10
create table t1 (c1 char(10) KEY, c2 char(10), key sec_idx(c2));
insert into t1 values ("r1c1", "r1c2");
select * from t1;
alter table t1 add column c3 char(10) default "def_c3";
select * from t1;
insert into t1 values ("r2c1", "r2c2", "r3c3");
drop table t1;

--echo # Scenario 11
--eval CREATE TABLE t1(c2 char(100) , FULLTEXT INDEX `idx1` (c2)) ENGINE=InnoDB ROW_FORMAT=$row_format
ALTER TABLE t1 DROP INDEX idx1;
DROP TABLE t1;

--echo # Scenario 12
--eval CREATE TABLE t1(c2 char(100)) ROW_FORMAT=$row_format
ALTER TABLE t1 ADD FULLTEXT INDEX `ft_idx` (c2);
DROP TABLE t1;

--echo # Scenario 13
--eval CREATE TABLE t1(a INT) ROW_FORMAT=$row_format
INSERT INTO t1 VALUES(1);
ALTER TABLE t1 add COLUMN (f INT generated always as (a+1) virtual, g INT );
ALTER TABLE t1 add COLUMN (h INT generated always as (a+1) virtual), add INDEX idx (h), algorithm=inplace;
DROP TABLE t1;

--echo # Scenario 14
--eval CREATE TABLE t1 (col1 int(11) DEFAULT NULL, col2 int(11) DEFAULT NULL, col3 int(11) DEFAULT NULL, col4 int(11) DEFAULT NULL, col5 int(11) GENERATED ALWAYS AS (col4 * col2) VIRTUAL, col6 int(11) GENERATED ALWAYS AS (col2 % col4) VIRTUAL, col7 int(11) GENERATED ALWAYS AS (col5 / col6) VIRTUAL, col8 int(11) GENERATED ALWAYS AS (col5 + col5) VIRTUAL, col9 text, extra int(11) DEFAULT NULL) ROW_FORMAT=$row_format

ALTER TABLE t1 DROP COLUMN col7;
ALTER TABLE t1 DROP COLUMN col6, DROP COLUMN col9;
DROP TABLE t1;

--echo # Scenario 15
create table t1(a text, b text, fulltext(a,b));
insert into t1 values('text1', 'text2'); insert into t1 values('test1', 'test2');
alter table t1 add fulltext(b);
drop table t1;

--echo # Scenario 16
--eval CREATE TABLE t1 (a serial key, b text) ROW_FORMAT=$row_format
INSERT INTO t1(b) VALUES(repeat("a short string - ",5));
INSERT INTO t1(b) VALUES(repeat("a long string - ",50));
INSERT INTO t1(b) SELECT b FROM t1;
ALTER TABLE t1 ADD COLUMN (c int), algorithm=INSTANT;
--error 1845
ALTER TABLE t1 ADD INDEX c (c), algorithm=INSTANT;
ALTER TABLE t1 ADD INDEX c (c), algorithm=INPLACE;
DROP TABLE t1;

--echo # Scenario 17
--eval CREATE TABLE t1 (c1 char(20), c2 char(20), c3 char(20), c4 char(20)) ROW_FORMAT=$row_format
ALTER TABLE t1 DROP COLUMN c4, ALGORITHM=INSTANT;
ALTER TABLE t1 DROP COLUMN c2, ALGORITHM=INSTANT;
ALTER TABLE test.t1 ADD c6 char(20), DROP COLUMN c3;
DROP TABLE t1;

--echo # Scenario 18
--eval CREATE TABLE t1 (c1 INT PRIMARY KEY, c2 INT DEFAULT 1, ct TEXT, INDEX(c2)) ROW_FORMAT=$row_format
ALTER TABLE t1 DISCARD TABLESPACE;
ALTER TABLE t1 ADD FULLTEXT INDEX (ct), ALGORITHM=INPLACE;
ALTER TABLE t1 CHANGE c2 c2 INT AFTER c1, ALGORITHM=INPLACE;
DROP TABLE t1;

--echo # Scenario 19
--eval CREATE TABLE t1 (a INT, b INT, c INT, d INT) ENGINE=InnoDB ROW_FORMAT=$row_format
ALTER TABLE t1 ADD COLUMN aa INT DEFAULT 1 FIRST;
ALTER TABLE t1 CHANGE aa e INT;
INSERT INTO t1 VALUES (1,1,1,1,1);
ALTER TABLE t1 ADD COLUMN a INT DEFAULT 1 FIRST, DROP COLUMN a, CHANGE b b INT;
ALTER TABLE t1 ADD COLUMN aa INT DEFAULT 1 AFTER a, DROP COLUMN b, CHANGE c c INT;
ALTER TABLE t1 CHANGE aa b INT;
DROP TABLE t1;

--echo # Scenario 20
--eval CREATE TABLE t1 (a varchar(512) NOT NULL, b varchar(512) NOT NULL, FULLTEXT KEY fts_idx (a)) ROW_FORMAT=$row_format
ALTER TABLE t1 DROP KEY fts_idx;
ALTER TABLE t1 ADD COLUMN b_hash char(64) AS(b) VIRTUAL NOT NULL;
DROP TABLE t1;

--echo # Scenario 21
Delimiter |;
CREATE PROCEDURE test.fill_table (IN start INT) BEGIN SET @idx =start; WHILE (@idx > 0) DO INSERT INTO test.t1 (v) VALUES("aaaaaaaaaaaaa"); SET @idx = @idx - 1; END WHILE; END|
Delimiter ;|
--eval CREATE TABLE t1(id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY) ROW_FORMAT=$row_format
ALTER TABLE t1 ADD COLUMN v VARCHAR(255) DEFAULT "default_value", ALGORITHM=INSTANT;
call fill_table(400);
DROP PROCEDURE test.fill_table;
DROP TABLE t1;

--echo # Scenario 22
--eval CREATE TABLE t1(id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY) ROW_FORMAT=$row_format
ALTER TABLE t1 ADD COLUMN v VARCHAR(255) DEFAULT "default_value", ALGORITHM=INSTANT;
SET GLOBAL innodb_limit_optimistic_insert_debug=2;
INSERT INTO test.t1 (v) VALUES("aaaaaaaaaaaaa");
INSERT INTO test.t1 (v) VALUES("aaaaaaaaaaaaa");
INSERT INTO test.t1 (v) VALUES("aaaaaaaaaaaaa");
SELECT * FROM t1;
SET GLOBAL innodb_limit_optimistic_insert_debug=0;
DROP TABLE t1;

--echo # Scenario 23
create table t1 (c1 char(10), c2 char(10));
--let $table_name=t1
--source suite/innodb/include/print_instant_metadata.inc
insert into t1 values ("r1c1", "r1c2");
select * from t1;
alter table t1 add column c3 char(10) default "def_c3" after c1;
--source suite/innodb/include/print_instant_metadata.inc
insert into t1 values ("r2c1", "r2c3", "r2c2");
select * from t1;
alter table t1 add column c3 char(10) default "newnewnew", drop column c3;
select * from t1;
DROP TABLE t1;

--echo # Scenario 24
--eval CREATE TABLE t1 (c1 char(10)) ROW_FORMAT=$row_format
ALTER TABLE t1 ADD COLUMN c2 INT;
ALTER TABLE t1 DROP COLUMN c2;
INSERT INTO t1 (c1) VALUES("aaaa");
select * from t1;
check table t1;
DROP TABLE t1;

--echo # Scenario 25
--eval CREATE TABLE t1 (c1 int, c2 char(10)) ROW_FORMAT=$row_format PARTITION BY HASH (c1) PARTITIONS 2
alter table t1 drop column c2;
DROP TABLE t1;

--echo # Scenario 26
--eval CREATE TABLE t1 (c1 char(10) KEY, c2 char(10)) ROW_FORMAT=$row_format
INSERT INTO t1 values ("row1_c1", "row1_c2");
ALTER TABLE t1 ADD COLUMN c3 char(20) DEFAULT "default_c3", ALGORITHM=INSTANT;
INSERT INTO t1 values ("row2_c1", "row2_c2", "row2_c3");
ALTER TABLE t1 ADD COLUMN c4 char(20) DEFAULT NULL AFTER c1, ALGORITHM=INSTANT;
INSERT INTO t1 values ("row3_c1", "row3_c4", "row3_c2", "row3_c3");
ALTER TABLE t1 force;
SELECT * FROM t1;
ALTER TABLE t1 DROP COLUMN c3, ALGORITHM=INSTANT;
ALTER TABLE t1 ADD COLUMN c5 INT after c2, ALGORITHM=INSTANT;
ALTER TABLE t1 DROP COLUMN c2, ALGORITHM=INSTANT;
DROP TABLE t1;

--echo # Scenario 27
--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format
ALTER TABLE t1 ADD COLUMN c INT NOT NULL;
ALTER TABLE t1 ADD COLUMN d VARCHAR(100);
INSERT INTO t1 VALUES(0, 5, 20, 'Hello world');
DROP TABLE t1;

--echo # Scenario 28
--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format
INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5);
SELECT * from t1;
ALTER TABLE t1 ADD COLUMN c INT NOT NULL DEFAULT 100, ADD COLUMN d INT GENERATED ALWAYS AS ((c * 2 + b)) VIRTUAL, ADD COLUMN e VARCHAR(100) DEFAULT 'Hello world';
SELECT * from t1;
UPDATE t1 SET c = 200 WHERE a > 3;
SELECT * from t1;
DROP TABLE t1;

--echo # Scenario 29
--eval CREATE TABLE t1 (c1 INT NOT NULL AUTO_INCREMENT PRIMARY KEY, c2 INT) ROW_FORMAT=$row_format;
INSERT INTO t1 VALUES(0, 1);
SELECT * from t1;
ALTER TABLE t1 ADD COLUMN c3 INT, ALGORITHM=INSTANT;
SELECT * from t1;
ALTER TABLE t1 ADD COLUMN c4 CHAR(100), ALGORITHM=INSTANT;
SELECT * from t1;
UPDATE t1 SET c3 = 2;
SELECT * from t1;
DROP TABLE t1;

--echo # Scenario 30
--eval CREATE TABLE t1 (c1 char(10), c2 char(10)) ROW_FORMAT=$row_format;
INSERT INTO t1 VALUES("r1c1", "r1c2");
SELECT * from t1;
ALTER TABLE t1 ADD COLUMN c3 INT, ALGORITHM=INSTANT;
SELECT * from t1;
ALTER TABLE t1 ADD COLUMN c4 char(100), ALGORITHM=INSTANT;
SELECT * from t1;
UPDATE t1 SET c3 = 2;
SELECT * from t1;
DROP TABLE t1;

--echo # Scenario 31
--eval CREATE TABLE t1 (c1 char(10), c2 char(10)) ROW_FORMAT=$row_format
INSERT INTO t1 VALUES("r1c1", "r1c2");
ALTER TABLE t1 ADD COLUMN c3 char(10) default "def_c3";
SELECT * from t1;
UPDATE t1 SET c3 = "new_c3";
SELECT * from t1;
UPDATE t1 SET c3 = "new2_c3";
SELECT * from t1;
DROP TABLE t1;

--echo # Scenario 32
--eval CREATE TABLE t1 (a INT NOT NULL, b INT NOT NULL, PRIMARY KEY(a, b)) ROW_FORMAT=$row_format
INSERT INTO t1 VALUES(0, 1), (1, 2), (2, 3), (3, 4);
ALTER TABLE t1 ADD COLUMN c INT NOT NULL DEFAULT 5;
CHECK TABLE t1;
SELECT * FROM t1;
UPDATE t1 SET c = b WHERE b <= 2;
CHECK TABLE t1;
SELECT * FROM t1;
DROP TABLE t1;

--echo # Scenario 33
--eval CREATE TABLE t1(c1 longtext) ROW_FORMAT=$row_format
INSERT INTO t1 VALUES (repeat('x', 8098));
SELECT SUBSTRING(c1, 1000, 32) FROM t1;
ALTER TABLE t1 ADD COLUMN c2 INT, ALGORITHM=INSTANT;
SELECT SUBSTRING(c1, 1000, 32), c2 FROM t1;
BEGIN;
UPDATE t1 SET c2 = 0;
SELECT SUBSTRING(c1, 1000, 32), c2 FROM t1;
ROLLBACK;
SELECT SUBSTRING(c1, 1000, 32), c2 FROM t1;
DROP TABLE t1;

--echo # Scenario 34
--eval CREATE TABLE t1 (c1 char(10), c2 char(10)) ROW_FORMAT=$row_format
INSERT INTO t1 VALUES("r1c1", "r1c2");
SELECT * FROM t1;
ALTER TABLE t1 ADD COLUMN c3 char(10) DEFAULT "c3_def", ALGORITHM=INSTANT;
SELECT * FROM t1;
SET GLOBAL innodb_log_checkpoint_now = ON;
START TRANSACTION;
INSERT INTO t1 VALUES("r2c1", "r2c2", "r2c3");
SELECT * FROM t1;
--source include/expect_crash.inc
SET SESSION DEBUG="+d,crash_commit_after";
--error 2013
COMMIT;
--source include/start_mysqld.inc
CHECK TABLE t1;
SELECT * FROM t1;
INSERT INTO t1 VALUES("r3c1", "r3c2", "r3c3");
START TRANSACTION;
UPDATE t1 SET c2 = "temp";
SELECT * FROM t1;
--source include/expect_crash.inc
SET SESSION DEBUG="+d,crash_commit_after";
--error 2013
COMMIT;
--source include/start_mysqld.inc
CHECK TABLE t1;
SELECT * FROM t1;
DROP TABLE t1;

--echo # Scenario 35
--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format
INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5);
ALTER TABLE t1 ADD COLUMN c1 INT DEFAULT 10, algorithm=INSTANT;
CHECK TABLE t1;
START TRANSACTION;
INSERT INTO t1 VALUES(0, 6, 20), (0, 7, 20);
SELECT * FROM t1;
--source include/expect_crash.inc
SET SESSION DEBUG="+d,crash_commit_after";
--error 2013
COMMIT;
--source include/start_mysqld.inc
CHECK TABLE t1;
SELECT * FROM t1;
INSERT INTO t1 VALUES(0, 6, 20), (0, 7, 20);
START TRANSACTION;
UPDATE t1 SET b = 10;
SELECT * FROM t1;
--source include/expect_crash.inc
SET SESSION DEBUG="+d,crash_commit_after";
--error 2013
COMMIT;
--source include/start_mysqld.inc
CHECK TABLE t1;
SELECT * FROM t1;
DROP TABLE t1;

--echo # Scenario 36
--eval CREATE TABLE t1 (c1 char(10), c2 char(10)) ROW_FORMAT=$row_format
INSERT INTO t1 VALUES("r1c1", "r1c2");
SELECT * FROM t1;
ALTER TABLE t1 ADD COLUMN c3 char(10) DEFAULT "c3_def", ALGORITHM=INSTANT;
INSERT INTO t1 VALUES("r2c1", "r2c2", "r2c3");
SELECT * FROM t1;
START TRANSACTION;
UPDATE t1 SET c2 = "temp";
SELECT * FROM t1;
--source include/expect_crash.inc
SET SESSION DEBUG="+d,crash_commit_after";
--error 2013
COMMIT;
--source include/start_mysqld.inc
CHECK TABLE t1;
SELECT * FROM t1;
DROP TABLE t1;

--echo # Scenario 37
--eval CREATE TABLE t1 (c1 char(10), c2 char(10)) ROW_FORMAT=$row_format
INSERT INTO t1 VALUES("r1c1", "r1c2");
SELECT * FROM t1;
ALTER TABLE t1 ADD COLUMN c3 char(10) DEFAULT "c3_def", ALGORITHM=INSTANT;
INSERT INTO t1 VALUES("r2c1", "r2c2", "r2c3");
SELECT * FROM t1;
SET GLOBAL innodb_log_checkpoint_now = ON;
START TRANSACTION;
UPDATE t1 SET c3 = "temp";
SELECT * FROM t1;
--source include/expect_crash.inc
SET SESSION DEBUG="+d,crash_commit_after";
--error 2013
COMMIT;
--source include/start_mysqld.inc
CHECK TABLE t1;
SELECT * FROM t1;
DROP TABLE t1;

--echo # Scenario 38
--eval CREATE TABLE t1 (col1 INT, col2 INT, col3 INT, col4 TEXT) ROW_FORMAT=$row_format PARTITION BY RANGE(col1 * 2) ( PARTITION p0 VALUES LESS THAN (128), PARTITION p1 VALUES LESS THAN (256) , PARTITION p2 VALUES LESS THAN (384) , PARTITION p3 VALUES LESS THAN MAXVALUE);
INSERT INTO t1 VALUES(1, 2, 3, 'abcdefg'), (100, 200, 300, 'qwerty'), (200, 300, 400, 'asdfg');
ALTER TABLE t1 ALGORITHM DEFAULT, ADD COLUMN col5 VARCHAR(500) default "def", ADD COLUMN col6 TEXT;
CHECK TABLE t1;
SELECT count(*) AS `Expect 4` FROM information_schema.innodb_tables WHERE instant_cols != 0;
ALTER TABLE t1 ALGORITHM INPLACE, REORGANIZE PARTITION p0 INTO (PARTITION p0_a VALUES LESS THAN (64), PARTITION p0_b VALUES LESS THAN (128));
SELECT count(*) AS `Expect 3` FROM information_schema.innodb_tables WHERE instant_cols != 0;
CHECK TABLE t1;
ALTER TABLE t1 ALGORITHM DEFAULT, ADD KEY idx4(col4(10));
SELECT count(*) AS `Expect 3` FROM information_schema.innodb_tables WHERE instant_cols != 0;
CHECK TABLE t1;
ALTER TABLE t1 ALGORITHM DEFAULT, LOCK EXCLUSIVE, REORGANIZE PARTITION p0_a, p0_b INTO (PARTITION p0 VALUES LESS THAN (128) TABLESPACE innodb_file_per_table);
SELECT count(*) AS `Expect 3` FROM information_schema.innodb_tables WHERE instant_cols != 0;
CHECK TABLE t1;
ALTER TABLE t1 ADD KEY idx3(col3);
SELECT count(*) AS `Expect 3` FROM information_schema.innodb_tables WHERE instant_cols != 0;
CHECK TABLE t1;
SELECT * FROM t1;
DROP TABLE t1;

--echo # Scenario 39
--eval CREATE TABLE t1 (c1 INT, c2 char(10), c3 char(10)) ROW_FORMAT=$row_format PARTITION BY RANGE(c1) ( PARTITION p0 VALUES LESS THAN (100), PARTITION p1 VALUES LESS THAN (200) , PARTITION p2 VALUES LESS THAN MAXVALUE);
INSERT INTO t1 VALUES(1, "r1c2", "r1c3"), (100, "r2c2", "r2c3"), (200, "r3c2", "r3c3");
SELECT * FROM t1;
SELECT NAME, N_COLS, TOTAL_ROW_VERSIONS, INITIAL_COLUMN_COUNTS, CURRENT_COLUMN_COUNTS, TOTAL_COLUMN_COUNTS  FROM information_schema.innodb_tables WHERE name like "%t1%";
ALTER TABLE t1 ALGORITHM INSTANT, ADD COLUMN col4 CHAR(10) DEFAULT "def_4";
SELECT * FROM t1;
CHECK TABLE t1;
SELECT NAME, N_COLS, TOTAL_ROW_VERSIONS, INITIAL_COLUMN_COUNTS, CURRENT_COLUMN_COUNTS, TOTAL_COLUMN_COUNTS  FROM information_schema.innodb_tables WHERE name like "%t1%";
ALTER TABLE t1 ALGORITHM INPLACE, REORGANIZE PARTITION p0 INTO (PARTITION p0_a VALUES LESS THAN (50), PARTITION p0_b VALUES LESS THAN (100));
SELECT * FROM t1;
SELECT NAME, N_COLS, TOTAL_ROW_VERSIONS, INITIAL_COLUMN_COUNTS, CURRENT_COLUMN_COUNTS, TOTAL_COLUMN_COUNTS  FROM information_schema.innodb_tables WHERE name like "%t1%";
CHECK TABLE t1;
ALTER TABLE t1 ALGORITHM DEFAULT, ADD KEY idx4(col4(10));
SELECT NAME, N_COLS, TOTAL_ROW_VERSIONS, INITIAL_COLUMN_COUNTS, CURRENT_COLUMN_COUNTS, TOTAL_COLUMN_COUNTS  FROM information_schema.innodb_tables WHERE name like "%t1%";
CHECK TABLE t1;
--error 1520
ALTER TABLE t1 ALGORITHM DEFAULT, LOCK EXCLUSIVE, REORGANIZE PARTITION p0_a, p0_b INTO (PARTITION p0 VALUES LESS THAN (128) TABLESPACE innodb_file_per_table);
SELECT NAME, N_COLS, TOTAL_ROW_VERSIONS, INITIAL_COLUMN_COUNTS, CURRENT_COLUMN_COUNTS, TOTAL_COLUMN_COUNTS  FROM information_schema.innodb_tables WHERE name like "%t1%";
CHECK TABLE t1;
ALTER TABLE t1 ADD KEY idx3(c3);
SELECT NAME, N_COLS, TOTAL_ROW_VERSIONS, INITIAL_COLUMN_COUNTS, CURRENT_COLUMN_COUNTS, TOTAL_COLUMN_COUNTS  FROM information_schema.innodb_tables WHERE name like "%t1%";
CHECK TABLE t1;
SELECT * FROM t1;
DROP TABLE t1;

--echo # Scenario 40
--eval CREATE TABLE t1 (c1 INT, c2 char(10), c3 char(10)) ROW_FORMAT=$row_format PARTITION BY RANGE(c1)  ( PARTITION p0 VALUES LESS THAN (100),  PARTITION p1 VALUES LESS THAN (200) , PARTITION p2 VALUES LESS THAN MAXVALUE);
INSERT INTO t1 VALUES(1, "r1c2", "r1c3"), (100, "r2c2", "r2c3"), (200, "r3c2", "r3c3");
SELECT * FROM t1;
ALTER TABLE t1 ALGORITHM INSTANT, ADD COLUMN col4 CHAR(10) DEFAULT "def_4";
CHECK TABLE t1;
SELECT * FROM t1;
ALTER TABLE t1 ALGORITHM INPLACE, REORGANIZE PARTITION p0 INTO (PARTITION p0_a VALUES LESS THAN (50), PARTITION p0_b VALUES LESS THAN (100));
CHECK TABLE t1;
SELECT * FROM t1;
ALTER TABLE t1 ALGORITHM INPLACE, ADD KEY idx4(col4);
CHECK TABLE t1;
SELECT * FROM t1;
--error 1520
ALTER TABLE t1 ALGORITHM DEFAULT, LOCK EXCLUSIVE, REORGANIZE PARTITION p0_a, p0_b INTO (PARTITION p0 VALUES LESS THAN (128) TABLESPACE innodb_file_per_table);
CHECK TABLE t1;
SELECT * FROM t1;
ALTER TABLE t1 ADD KEY idx3(c3);
CHECK TABLE t1;
SELECT * FROM t1;
DROP TABLE t1;

--echo # Scenario 41
--eval CREATE TABLE t1 (col1 INT, col2 INT, col3 INT, col4 TEXT) ROW_FORMAT=$row_format PARTITION BY RANGE(col1 * 2)  (PARTITION p0 VALUES LESS THAN (128), PARTITION p1 VALUES LESS THAN (256), PARTITION p2 VALUES LESS THAN (384), PARTITION p3 VALUES LESS THAN MAXVALUE);
INSERT INTO t1 VALUES(1, 2, 3, 'abcdefg'), (100, 200, 300, 'qwerty'), (200, 300, 400, 'asdfg');
ALTER TABLE t1 ALGORITHM DEFAULT, ADD COLUMN col5 VARCHAR(500) default "def", ADD COLUMN col6 TEXT;
CHECK TABLE t1;
SELECT * FROM t1;
ALTER TABLE t1 ALGORITHM INPLACE, REORGANIZE PARTITION p0 INTO (PARTITION p0_a VALUES LESS THAN (64), PARTITION p0_b VALUES LESS THAN (128));
CHECK TABLE t1;
SELECT * FROM t1;
ALTER TABLE t1 ALGORITHM DEFAULT, ADD KEY idx4(col4(10));
CHECK TABLE t1;
SELECT * FROM t1;
ALTER TABLE t1 ALGORITHM DEFAULT, LOCK EXCLUSIVE, REORGANIZE PARTITION p0_a, p0_b INTO (PARTITION p0 VALUES LESS THAN (128) TABLESPACE innodb_file_per_table);
CHECK TABLE t1;
SELECT * FROM t1;
ALTER TABLE t1 ADD KEY idx3(col3);
CHECK TABLE t1;
SELECT * FROM t1;
DROP TABLE t1;

--echo # Scenario 42
--eval CREATE TABLE t1 (c1 INT, c2 char(10), c3 char(10)) ROW_FORMAT=$row_format PARTITION BY RANGE(c1)  ( PARTITION p0 VALUES LESS THAN (100), PARTITION p1 VALUES LESS THAN (200) ,  PARTITION p2 VALUES LESS THAN MAXVALUE);
INSERT INTO t1 VALUES(1, "r1c2", "r1c3"), (100, "r2c2", "r2c3"), (200, "r3c2", "r3c3");
SELECT * FROM t1;
ALTER TABLE t1 ALGORITHM INSTANT, ADD COLUMN col4 CHAR(10) DEFAULT "def_4";
CHECK TABLE t1;
SELECT * FROM t1;
ALTER TABLE t1 ALGORITHM INSTANT, DROP COLUMN c2;
CHECK TABLE t1;
SELECT * FROM t1;
ALTER TABLE t1 ALGORITHM INPLACE, REORGANIZE PARTITION p0 INTO (PARTITION p0_a VALUES LESS THAN (50), PARTITION p0_b VALUES LESS THAN (100));
CHECK TABLE t1;
SELECT * FROM t1;
ALTER TABLE t1 ALGORITHM INPLACE, ADD KEY idx4(col4);
CHECK TABLE t1;
SELECT * FROM t1;
--error 1520
ALTER TABLE t1 ALGORITHM DEFAULT, LOCK EXCLUSIVE, REORGANIZE PARTITION p0_a, p0_b INTO (PARTITION p0 VALUES LESS THAN (128) TABLESPACE innodb_file_per_table);
CHECK TABLE t1;
SELECT * FROM t1;
ALTER TABLE t1 ADD KEY idx3(c3);
CHECK TABLE t1;
SELECT * FROM t1;
DROP TABLE t1;

--echo # Scenario 43
--eval CREATE TABLE t1 (a INT NOT NULL AUTO_INCREMENT PRIMARY KEY, b INT) ROW_FORMAT=$row_format PARTITION BY HASH(a) PARTITIONS 3;
INSERT INTO t1 VALUES(0, 1), (0, 2), (0, 3), (0, 4), (0, 5), (0, 6), (0, 7), (0, 8);
SELECT * FROM t1;
ALTER TABLE t1 ADD COLUMN c INT NOT NULL DEFAULT 5;
CHECK TABLE t1;
SELECT * FROM t1;
ALTER TABLE t1 ADD PARTITION PARTITIONS 10;
CHECK TABLE t1;
SELECT * FROM t1;
ALTER TABLE t1 ADD KEY(b);
CHECK TABLE t1;
SELECT * FROM t1;
DROP TABLE t1;

--echo # Scenario 44
--eval CREATE TABLE t1 (a BLOB, b BLOB, c BLOB GENERATED ALWAYS AS (CONCAT(a,b)) VIRTUAL, h VARCHAR(10) DEFAULT NULL) ROW_FORMAT=$row_format
INSERT INTO t1 VALUES (REPEAT('g', 16000), REPEAT('x', 16000), DEFAULT, "kk");
CREATE INDEX idx ON t1(c(100));
SELECT length(c) FROM t1;
START TRANSACTION;
INSERT INTO t1 VALUES (REPEAT('a', 16000), REPEAT('b', 16000), DEFAULT, 'mm');
ROLLBACK;
INSERT INTO t1 VALUES (REPEAT('a', 16000), REPEAT('b', 16000), DEFAULT, 'mm');
START TRANSACTION;
UPDATE t1 SET a = REPEAT('m', 16000) WHERE a like "aaa%";
ROLLBACK;
DROP TABLE t1;

--echo # Scenario 45
--eval CREATE TABLE t1 (a INT, b INT, c INT GENERATED ALWAYS AS(a+b), h VARCHAR(10), j INT, m INT  GENERATED ALWAYS AS(b + j), n VARCHAR(10), p VARCHAR(20) GENERATED ALWAYS AS(CONCAT(n, h)), INDEX idx1(c), INDEX idx2 (m), INDEX idx3(p)) ROW_FORMAT=$row_format
INSERT INTO t1 VALUES(11, 22, DEFAULT, "AAA", 8, DEFAULT, "XXX", DEFAULT);
INSERT INTO t1 VALUES(1, 2, DEFAULT, "uuu", 9, DEFAULT, "uu", DEFAULT);
INSERT INTO t1 VALUES(3, 4, DEFAULT, "uooo", 1, DEFAULT, "umm", DEFAULT);
ALTER TABLE t1 DROP COLUMN c, algorithm=inplace;
ALTER TABLE t1 DROP COLUMN p, ADD COLUMN s VARCHAR(20) GENERATED ALWAYS AS(CONCAT(n, h)), algorithm=inplace;
DROP TABLE t1;

--echo # Scenario 46
connect (conn1, localhost, root,,);
connection default;
--eval CREATE TABLE t1 (a INT NOT NULL PRIMARY KEY, b INT, c INT) ROW_FORMAT=$row_format
INSERT INTO t1 VALUES(1, 1, 1), (2, 2, 2), (3, 3, 3), (6, 6, 6);
ALTER TABLE t1 ADD COLUMN d INT DEFAULT 10;
SELECT * FROM t1;
SET DEBUG_SYNC = 'row_log_table_apply1_before SIGNAL altered WAIT_FOR dmls_done';
--send ALTER TABLE t1 DROP COLUMN c, algorithm=inplace;
connection conn1;
SET DEBUG_SYNC = 'now WAIT_FOR altered';
INSERT INTO t1(a, b, c) VALUES(7, 7, 7);
UPDATE t1 SET a = 10 WHERE a = 1;
DELETE FROM t1 WHERE a = 2;
UPDATE t1 SET c = 10 WHERE a = 3;
UPDATE t1 SET d = 20 WHERE a = 5;
UPDATE t1 SET a = 8, d = 20 WHERE a = 6;
SET DEBUG_SYNC = 'now SIGNAL dmls_done';
disconnect conn1;
connection default;
reap;
SELECT * FROM t1;
DROP TABLE t1;

--echo # Scenario 47
--eval CREATE TABLE t1 (a INT NOT NULL PRIMARY KEY, b INT) ROW_FORMAT=$row_format PARTITION BY RANGE (a) (PARTITION p1 VALUES LESS THAN (10), PARTITION p2 VALUES LESS THAN (20))
ALTER TABLE t1 ADD COLUMN c INT DEFAULT 100;
SELECT NAME, POS, MTYPE, PRTYPE, LEN, HAS_DEFAULT, DEFAULT_VALUE, VERSION_ADDED, VERSION_DROPPED, PHYSICAL_POS FROM information_schema.innodb_columns WHERE has_default = 1;
ALTER TABLE t1 REORGANIZE PARTITION p2 INTO (PARTITION p21 VALUES LESS THAN(15), PARTITION p22 VALUES LESS THAN(20));
SELECT NAME, POS, MTYPE, PRTYPE, LEN, HAS_DEFAULT, DEFAULT_VALUE, VERSION_ADDED, VERSION_DROPPED, PHYSICAL_POS FROM information_schema.innodb_columns WHERE has_default = 1;
DROP TABLE t1;

--echo # Scenario 48
--eval CREATE TABLE t1 (c1 integer PRIMARY KEY AUTO_INCREMENT, c2 integer NULL UNIQUE) ROW_FORMAT=$row_format
ALTER TABLE t1 ADD COLUMN c3 int;
ALTER TABLE t1 DROP COLUMN c3;
ALTER TABLE t1 ADD COLUMN c4 int DEFAULT 1;
ALTER TABLE t1 CHANGE COLUMN c4 c4_new int NULL DEFAULT 1;
ALTER TABLE t1 CHANGE COLUMN c4_new c4_new int NULL DEFAULT 1;
DROP TABLE t1;

--echo # Scenario 49
--eval CREATE TABLE t1 (c1 char(20) KEY, c2 enum('a', 'b', 'c')) ROW_FORMAT=$row_format
INSERT INTO t1 values ("row1_c1", "a");
SELECT * from t1;
ALTER TABLE t1 ADD COLUMN c3 enum('x', 'y', 'z'), ALGORITHM=INSTANT;
SELECT * from t1;
INSERT INTO t1 values ("row2_c2", "b", "y");
SELECT * from t1;
ALTER TABLE t1 DROP COLUMN c2, ALGORITHM=INSTANT;
SELECT * from t1;
DROP TABLE t1;

--echo # Scenario 50
--eval CREATE TABLE t1 (c1 char(10), c2 char(10)) ROW_FORMAT=$row_format
ALTER TABLE t1 DROP COLUMN c2;
truncate table t1;
truncate table t1;
truncate table t1;
DROP TABLE t1;

--echo # Scenario 51
--eval CREATE TABLE t1 (c1 int KEY, c2 int) ROW_FORMAT=$row_format PARTITION BY RANGE(c1) (PARTITION p0 VALUES LESS THAN (10), PARTITION p1 VALUES LESS THAN (20), PARTITION p2 VALUES LESS THAN (30))
ALTER TABLE t1 DROP COLUMN c2, ALGORITHM=INSTANT;
ALTER TABLE t1 DROP PARTITION p1;
DROP TABLE t1;

--echo # Scenario 52
--eval CREATE TABLE t1(c1 int, c2 int, c3 int, c4 DOUBLE GENERATED ALWAYS AS (c1+c2) STORED, c5 DOUBLE GENERATED ALWAYS AS (c1-c2) VIRTUAL) ROW_FORMAT=$row_format;
INSERT INTO t1(c1,c2,c3) VALUES (1,1,1);
SELECT * FROM t1;
--error ER_DEPENDENT_BY_GENERATED_COLUMN
ALTER TABLE t1 DROP COLUMN c2, ALGORITHM=INSTANT;
ALTER TABLE t1 DROP COLUMN c3, ALGORITHM=INSTANT;
SELECT * FROM t1;
ALTER TABLE t1 DROP COLUMN c4, ALGORITHM=INSTANT;
SELECT * FROM t1;
ALTER TABLE t1 ADD COLUMN c6 DOUBLE GENERATED ALWAYS AS (2*c1+c2) VIRTUAL, ALGORITHM=INSTANT;
SELECT * FROM t1;
--error ER_ALTER_OPERATION_NOT_SUPPORTED
ALTER TABLE t1 ADD COLUMN c7 DOUBLE GENERATED ALWAYS AS (c1+2*c2) STORED, ALGORITHM=INSTANT;
SELECT * FROM t1;
--error ER_ALTER_OPERATION_NOT_SUPPORTED_REASON
ALTER TABLE t1 ADD COLUMN c8 DOUBLE GENERATED ALWAYS AS (c1+2*c2) VIRTUAL FIRST, ALGORITHM=INSTANT;
SELECT * FROM t1;
ALTER TABLE t1 add column c3 int after c2, ALGORITHM=INSTANT;
SELECT * FROM t1;
ALTER TABLE t1 DROP column c3, add column c10 int as (c1+c2) virtual, ALGORITHM=INSTANT;
SELECT * FROM t1;
ALTER TABLE t1 add column c3 int after c2, ALGORITHM=INSTANT;
SELECT * FROM t1;
ALTER TABLE t1 DROP column c3, drop column c6, ALGORITHM=INSTANT;
SELECT * FROM t1;
ALTER TABLE t1 add column c3 int after c2, ALGORITHM=INSTANT;
SELECT * FROM t1;
--error ER_ALTER_OPERATION_NOT_SUPPORTED_REASON
ALTER TABLE t1 DROP column c3, add column c11 int as (c1+c2) virtual first, ALGORITHM=INSTANT;
SELECT * FROM t1;
DROP TABLE t1;

--echo # Scenario 53
--echo # Table with functional index.
--eval CREATE TABLE t1 (id int NOT NULL AUTO_INCREMENT KEY, c1 varchar(10) DEFAULT NULL, c2 varchar(10) DEFAULT NULL, INDEX idx((concat(c1, "c1")))) ROW_FORMAT=$row_format;
INSERT INTO t1(c1, c2) values("abc", "def");
SELECT * FROM t1;
--error ER_ALTER_OPERATION_NOT_SUPPORTED_REASON
ALTER TABLE t1 DROP COLUMN c2, ADD COLUMN c3 varchar(10) as (c1) virtual after id, ALGORITHM=INSTANT;
--error ER_ALTER_OPERATION_NOT_SUPPORTED_REASON
ALTER TABLE t1 DROP COLUMN c2, ADD COLUMN c3 varchar(10) as (c1) virtual, ALGORITHM=INSTANT;
ALTER TABLE t1 DROP COLUMN c2, ALGORITHM=INSTANT;
SELECT * FROM t1;
DROP TABLE t1;

--echo # Scenarion 54
--eval CREATE TABLE t1 (c1 TEXT, c2 INT, PRIMARY KEY (c1(1))) ROW_FORMAT=$row_format;
SELECT * FROM t1;
REPLACE INTO t1(c1) VALUES ('');
REPLACE INTO t1(c1) VALUES ('');
SELECT * From t1;
ALTER TABLE t1 DROP COLUMN c2, ALGORITHM=INSTANT;
REPLACE INTO t1(c1) VALUES ('');
DROP TABLE t1;

--echo # Scenario 55
--eval CREATE TABLE t1 (c1 INT PRIMARY KEY, c2 INT) ROW_FORMAT=$row_format PARTITION BY RANGE(c1) (PARTITION p0 VALUES LESS THAN (10), PARTITION p1 VALUES LESS THAN (20))

ALTER TABLE t1 ADD COLUMN c3 INT, ALGORITHM=INSTANT;
ALTER TABLE t1 DROP COLUMN c2, ALGORITHM=INSTANT;

ALTER TABLE t1 ADD COLUMN c2 INT, ALGORITHM=INSTANT;
ALTER TABLE t1 DROP COLUMN c3, ALGORITHM=INSTANT;

ALTER TABLE t1 ADD PARTITION (PARTITION p5 VALUES LESS THAN (60));

ALTER TABLE t1 ADD COLUMN c3 INT, ALGORITHM=INSTANT;
ALTER TABLE t1 DROP COLUMN c2, ALGORITHM=INSTANT;

ALTER TABLE t1 REORGANIZE PARTITION p5 INTO
(
    PARTITION p51 VALUES LESS THAN (50),
    PARTITION p52 VALUES LESS THAN (60)
);

DROP TABLE t1;

--echo # Scenario 56
--eval CREATE TABLE t1 (c1 INT, c2 INT, c3 INT, c4 TEXT, PRIMARY KEY(c1, c4(10))) ROW_FORMAT=$row_format;
INSERT INTO t1 VALUES (1, 2, 3, "abcdefghijklmnop");
SELECT * FROM t1;

ALTER TABLE t1 ADD COLUMN c5 INT, ALGORITHM=INSTANT;
SELECT * FROM t1;

ALTER TABLE t1 DROP COLUMN c2, ALGORITHM=INSTANT;
SELECT * FROM t1;

DROP TABLE t1;

--echo # Scenario 57
--eval CREATE TABLE t1 (c1 CHAR(20), c2 CHAR(20), PRIMARY KEY PK(c1(4))) ROW_FORMAT=$row_format;
INSERT INTO t1 VALUES ("r1c1", "r1c2");
SELECT * FROM t1;
ALTER TABLE t1 ADD COLUMN c3 char(20) DEFAULT "c3_def", ALGORITHM=INSTANT;
INSERT INTO t1 VALUES ("r2c1", "r2c2", "r2c3");
SELECT * FROM t1;
DROP TABLE t1;

--echo # Scenario 58
--echo # Validating record in table with INSTANT ADD/DROP columns
eval CREATE TABLE t1 (c1 CHAR(10), c2 CHAR(10)) ROW_FORMAT=$row_format;
INSERT INTO t1 VALUES ("r1c1", "r1c2");
ALTER TABLE t1 ADD COLUMN c3 CHAR(10), ALGORITHM=INSTANT;
INSERT INTO t1 VALUES ("r2c1", "r2c2", "r2c3");
ALTER TABLE t1 DROP COLUMN c1, ALGORITHM=INSTANT;
INSERT INTO t1 (c2) VALUES ("r3c2");
ALTER TABLE t1 ADD COLUMN c4 CHAR(10), ALGORITHM=INSTANT;
INSERT INTO t1 (c2) VALUES ("r4c2");
SELECT * From t1;
DROP TABLE t1;

eval CREATE TABLE t1 (id INT KEY, c2 CHAR(10)) ROW_FORMAT=$row_format PARTITION BY RANGE( id ) (PARTITION P0 VALUES LESS THAN (40), PARTITION P1 VALUES LESS THAN (50));
INSERT INTO t1 VALUES(1,"r1c2");
INSERT INTO t1 VALUES(2,"r2c2");
ALTER TABLE t1 DROP COLUMN c2, ALGORITHM=INSTANT;
ALTER TABLE t1 ADD COLUMN c2 CHAR(10), ALGORITHM=INSTANT;
ALTER TABLE t1 REORGANIZE PARTITION P0, P1 INTO (PARTITION P1 VALUES LESS THAN (50));
SELECT * FROM t1;
DROP TABLE t1;

--echo # Scenario 59
--echo # A scenario when #fields were not updated correctly in REDUNDANT record for not-inplace update.
eval CREATE TABLE t1 (c1 VARCHAR(20), c2 CHAR(20)) ROW_FORMAT=$row_format;
INSERT INTO t1 VALUES ("r1c1", "r1c2");
SELECT * FROM t1;
ALTER TABLE t1 DROP COLUMN c2, ALGORITHM=INSTANT;
UPDATE t1 SET c1="r1c11111111111";
SELECT * FROM t1;
DROP TABLE t1;

--echo # Scenario 60
--echo # When a new partition is added in a table with instant columns and virtual column,
--echo # during inheriting INSTANT metadat for new partitions, virtual columns were not skipped.
eval CREATE TABLE t1 (col1 INT, col2 INT, col3 INT, col4 TEXT) ROW_FORMAT=$row_format PARTITION BY RANGE(col1 * 2) (PARTITION p0 VALUES LESS THAN (100), PARTITION p1 VALUES LESS THAN (200), PARTITION p2 VALUES LESS THAN (300));

ALTER TABLE t1 ADD COLUMN col5 INT, ADD COLUMN col6 INT GENERATED ALWAYS AS (col5 + 1) VIRTUAL, ALGORITHM=INSTANT;
ALTER TABLE t1 DROP COLUMN col2, algorithm=INSTANT;

ALTER TABLE t1 REORGANIZE PARTITION p2 INTO(PARTITION p21 VALUES LESS THAN (250), PARTITION p22 VALUES LESS THAN (300));
DROP TABLE t1;

--echo # Scenario 61
--echo # When a table is truncated, it's instant metadata is reset. It shall
--echo # also reset instant metadata of columns.
eval CREATE TABLE t1 (c1 VARCHAR(1008)) ROW_FORMAT=$row_format;

ALTER TABLE t1 ADD COLUMN c2 INT;
TRUNCATE TABLE t1;

INSERT INTO t1 (c1, c2) VALUES (CONCAT(REPEAT('z', 1008)),1);
INSERT INTO t1 (c1, c2) VALUES (CONCAT(REPEAT('z', 1008)),2);
INSERT INTO t1 (c1, c2) VALUES (CONCAT(REPEAT('z', 1008)),3);
INSERT INTO t1 (c1, c2) VALUES (CONCAT(REPEAT('z', 1008)),4);
INSERT INTO t1 (c1, c2) VALUES (CONCAT(REPEAT('z', 1008)),5);
INSERT INTO t1 (c1, c2) VALUES (CONCAT(REPEAT('z', 1008)),6);
INSERT INTO t1 (c1, c2) VALUES (CONCAT(REPEAT('z', 1008)),7);
INSERT INTO t1 (c1, c2) VALUES (CONCAT(REPEAT('z', 1008)),8);

ALTER TABLE t1 ADD COLUMN c3 VARCHAR(500);
UPDATE t1 SET c3 = CONCAT(REPEAT('c', 500));

DROP TABLE t1;

--echo # Scenario 62
--echo # While updating lob columns on a rebuilt table which had INSTANT cols
eval CREATE TABLE t1 (c1 CHAR(10)) ROW_FORMAT=$row_format;
INSERT INTO t1 VALUES ("r1c1");
SELECT * FROM t1;

ALTER TABLE t1 ADD COLUMN c2 INT;
ALTER TABLE t1 DROP COLUMN c2;

ALTER TABLE t1 ADD COLUMN c3 TEXT;
UPDATE t1 SET c3 = CONCAT(REPEAT('z', 8204));

ALTER TABLE t1 ADD PRIMARY KEY (c3(10)), ADD FULLTEXT KEY idx (c3);
SELECT * FROM t1;

DROP TABLE t1;

--echo # Scenario 63
--echo # Truncate of a partition/partition table
eval CREATE TABLE t1 (a INT NOT NULL PRIMARY KEY, b INT) ROW_FORMAT=$row_format PARTITION BY RANGE (a) (PARTITION p1 VALUES LESS THAN (10), PARTITION p2 VALUES LESS THAN (20));
--let $table_name=t1
--source suite/innodb/include/print_instant_part_metadata.inc

INSERT INTO t1 values (5, 50), (15, 150);
SELECT * FROM t1 PARTITION (p1);
SELECT * FROM t1 PARTITION (p2);

ALTER TABLE t1 ADD COLUMN c INT DEFAULT 100;
--source suite/innodb/include/print_instant_part_metadata.inc

ALTER TABLE t1 ADD PARTITION (PARTITION p3 VALUES LESS THAN(30));
INSERT INTO t1 (a, b) values (25, 250);

SELECT * FROM t1 PARTITION (p1);
SELECT * FROM t1 PARTITION (p2);
SELECT * FROM t1 PARTITION (p3);

alter table t1 drop column b, algorithm=instant;
--source suite/innodb/include/print_instant_part_metadata.inc
SELECT * FROM t1;

ALTER TABLE t1 TRUNCATE PARTITION p1;
--source suite/innodb/include/print_instant_part_metadata.inc
SELECT * FROM t1;

ALTER TABLE t1 TRUNCATE PARTITION p3;
--source suite/innodb/include/print_instant_part_metadata.inc
SELECT * FROM t1;

TRUNCATE TABLE t1;
--source suite/innodb/include/print_instant_part_metadata.inc
SELECT * FROM t1;

TRUNCATE TABLE t1;
ALTER TABLE t1 TRUNCATE PARTITION p1;
ALTER TABLE t1 TRUNCATE PARTITION p3;
--source suite/innodb/include/print_instant_part_metadata.inc
SELECT * FROM t1;

DROP TABLE t1;

--echo # Scenario 64
--echo # When nullbitmap size on physical record changes after INSTANT ADD columns.
eval CREATE TABLE t1 (c0 char(9) KEY, c1 char (10), c2 char(20), c3 char(30), c4 char(40), c5 char(50), c6 char(60), c7 char(70), c8 char(80)) ROW_FORMAT=$row_format;

SET GLOBAL innodb_limit_optimistic_insert_debug=2;

Insert into t1 values(
 REPEAT("a", 8),
 REPEAT("1", 9),
 REPEAT("2", 19),
 REPEAT("3", 29),
 REPEAT("4", 39),
 REPEAT("5", 49),
 REPEAT("6", 59),
 REPEAT("7", 69),
 REPEAT("8", 79));

Insert into t1 values(
 REPEAT("b", 8),
 REPEAT("1", 9),
 REPEAT("2", 19),
 REPEAT("3", 29),
 REPEAT("4", 39),
 REPEAT("5", 49),
 REPEAT("6", 59),
 REPEAT("7", 69),
 REPEAT("8", 79));

Alter table t1 add column c9 char(90);

Insert into t1 values(
 REPEAT("c", 8),
 REPEAT("1", 9),
 REPEAT("2", 19),
 REPEAT("3", 29),
 REPEAT("4", 39),
 REPEAT("5", 49),
 REPEAT("6", 59),
 REPEAT("7", 69),
 REPEAT("8", 79),
 REPEAT("9", 89));

SET GLOBAL innodb_limit_optimistic_insert_debug=0;

DROP table t1;

--echo # Scenario 65
--echo # Truncate all partition shall reset INSTANT Metadata
eval CREATE TABLE t1 (c1 INT, c2 INT, c3 INT, c4 TEXT) ROW_FORMAT=$row_format PARTITION BY RANGE(c1) (PARTITION p0 VALUES LESS THAN (100), PARTITION p1 VALUES LESS THAN (200), PARTITION p2 VALUES LESS THAN (300), PARTITION p3 VALUES LESS THAN MAXVALUE);

INSERT INTO t1 VALUES (90, 1, 1, "1"), (190, 1, 1, "2"), (290, 1, 1, "3"), (390, 1, 1, "3");

Alter table t1 add column c5 int, algorithm=INSTANT;
Alter table t1 drop column c3, algorithm=INSTANT;
--let $table_name=t1
--source suite/innodb/include/print_instant_part_metadata.inc

ALTER TABLE t1 TRUNCATE partition p0, p1, p2, p3;
--source suite/innodb/include/print_instant_part_metadata.inc

DROP TABLE t1;

--echo # Scenario 66
--echo # Purge crash
--eval create table t1 (c0 INT KEY, c1 varchar(10), c2 varchar(10)) ROW_FORMAT=$row_format

alter table t1 add column c4 char(10), algorithm=instant;
alter table t1 add index idx (c4);

insert into t1 values (1, "r1c1", "r1c2", "c4");
SET GLOBAL innodb_purge_stop_now=ON;
Update t1 set c2="r122222", c4="c44";

alter table t1 drop column c2, algorithm=instant;

SET GLOBAL innodb_purge_run_now=ON;
--source include/wait_innodb_all_purged.inc

DROP TABLE t1;

--echo # Scenario 66
--echo # Inplace update is failing
--echo # INSTANT ADD and INPLACE UPDATE
eval CREATE TABLE t1 (c1 char(10), c2 char(10), c3 char(10)) ROW_FORMAT=$row_format;

Insert into t1 values ("r1c1", "r1c2", "r1c3");
Select * from t1;

Alter table t1 add column c4 char(10) after c1, ALGORITHM=INSTANT;
Select * from t1;

Insert into t1 values ("r2c1", "r2c4", "r2c2", "r2c3");
Select * from t1;

--echo # Disable flush to make sure REDO is applied after restart
--disable_query_log
SET GLOBAL innodb_log_checkpoint_now = ON;
SET GLOBAL innodb_page_cleaner_disabled_debug = 1;
SET GLOBAL innodb_dict_stats_disabled_debug = 1;
SET GLOBAL innodb_master_thread_disabled_debug = 1;
--enable_query_log

Update t1 set c2="r112" where c2="r1c2";
Update t1 set c2="r212" where c2="r2c2";
Select * from t1;

--echo # Kill the running server
--source include/kill_mysqld.inc

--echo # Start the server
--source include/start_mysqld_no_echo.inc

Select * from t1;

DROP TABLE t1;

--echo # INSTANT DROP and INPLACE UPDATE
eval CREATE TABLE t1 (c1 char(10), c2 char(10), c3 char(10)) ROW_FORMAT=$row_format;

Insert into t1 values ("r1c1", "r1c2", "r1c3");
Select * from t1;

Alter table t1 drop column c1, algorithm=instant;
Insert into t1 values ("r2c2", "r2c3");
Select * from t1;


SET GLOBAL innodb_log_checkpoint_now = ON;
SET GLOBAL innodb_page_cleaner_disabled_debug = 1;
SET GLOBAL innodb_dict_stats_disabled_debug = 1;
SET GLOBAL innodb_master_thread_disabled_debug = 1;

Update t1 set c2="r112" where c2="r1c2";
Update t1 set c2="r212" where c2="r2c2";
Select * from t1;

--echo # Kill the running server
--source include/kill_mysqld.inc

--echo # Start the server
--source include/start_mysqld_no_echo.inc

Select * from t1;

DROP TABLE t1;

--echo # INSTANT ADD and DROP and INPLACE UPDATE
eval CREATE TABLE t1 (c1 char(10), c2 char(10), c3 char(10)) ROW_FORMAT=$row_format;

Insert into t1 values ("r1c1", "r1c2", "r1c3");
Select * from t1;

Alter table t1 add column c4 char(10) after c1, ALGORITHM=INSTANT;
Select * from t1;
Insert into t1 values ("r2c1", "r2c4", "r2c2", "r2c3");
Select * from t1;

Alter table t1 drop column c1, algorithm=instant;
Select * from t1;
Insert into t1 values ("r3c4", "r3c2", "r3c3");
Select * from t1;

SET GLOBAL innodb_log_checkpoint_now = ON;
SET GLOBAL innodb_page_cleaner_disabled_debug = 1;
SET GLOBAL innodb_dict_stats_disabled_debug = 1;
SET GLOBAL innodb_master_thread_disabled_debug = 1;

Update t1 set c2="r112" where c2="r1c2";
Update t1 set c2="r212" where c2="r2c2";
Update t1 set c2="r312" where c2="r3c2";
Select * from t1;

--echo # Kill the running server
--source include/kill_mysqld.inc

--echo # Start the server
--source include/start_mysqld_no_echo.inc

Select * from t1;

DROP TABLE t1;

--echo ###########
--echo # CLEANUP #
--echo ###########
