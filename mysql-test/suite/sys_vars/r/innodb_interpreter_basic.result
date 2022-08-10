SET @start_global_value = @@global.innodb_interpreter;
SELECT @start_global_value;
@start_global_value
init
select @@global.innodb_interpreter;
@@global.innodb_interpreter
init
select @@session.innodb_compression_level;
ERROR HY000: Variable 'innodb_compression_level' is a GLOBAL variable
show global variables like 'innodb_interpreter';
Variable_name	Value
innodb_interpreter	init
show session variables like 'innodb_interpreter';
Variable_name	Value
innodb_interpreter	init
select * from performance_schema.global_variables where variable_name='innodb_interpreter';
VARIABLE_NAME	VARIABLE_VALUE
innodb_interpreter	init
select * from performance_schema.session_variables where variable_name='innodb_interpreter';
VARIABLE_NAME	VARIABLE_VALUE
innodb_interpreter	init
CREATE TABLE t1 (f1 INT PRIMARY KEY);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  PRIMARY KEY (`f1`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
START TRANSACTION;
INSERT INTO t1(f1) VALUES (1), (2), (3), (4), (5);
SELECT * FROM t1;
f1
1
2
3
4
5
COMMIT;
# Ensure that dirty pages of table t1 is flushed.
FLUSH TABLES t1 FOR EXPORT;
UNLOCK TABLES;
SET SESSION innodb_interpreter = 'init';
SET SESSION innodb_interpreter = 'open_table test/t1';
SET SESSION innodb_interpreter = 'find_space_id test/t1';
SELECT @@session.innodb_interpreter_output INTO @space_id;
SET SESSION innodb_interpreter = CONCAT('find_fil_page_lsn ', @space_id, ' 0');
SELECT @@session.innodb_interpreter_output INTO @fil_page_lsn;
SET SESSION innodb_interpreter = 'make_ondisk_root_page_zeroes test/t1';
SET SESSION innodb_interpreter = 'destroy';
DROP TABLE t1;
CREATE TABLE t1 (f1 INT PRIMARY KEY);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  PRIMARY KEY (`f1`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
START TRANSACTION;
INSERT INTO t1(f1) VALUES (1), (2), (3), (4), (5);
SELECT * FROM t1;
f1
1
2
3
4
5
COMMIT;
# Ensure that dirty pages of table t1 is flushed.
FLUSH TABLES t1 FOR EXPORT;
UNLOCK TABLES;
SET SESSION innodb_interpreter = 'init';
SET SESSION innodb_interpreter = 'open_table test/t1';
SET SESSION innodb_interpreter = 'find_space_id test/t1';
SELECT @@session.innodb_interpreter_output INTO @space_id;
SET @cmd = CONCAT('find_fil_page_lsn ', @space_id, ' 0');
SET SESSION innodb_interpreter = @cmd;
SELECT @@session.innodb_interpreter_output INTO @fil_page_lsn;
SET SESSION innodb_interpreter = 'corrupt_ondisk_root_page test/t1';
SET SESSION innodb_interpreter = 'destroy';
DROP TABLE t1;
CREATE TABLE t1 (f1 INT PRIMARY KEY);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  PRIMARY KEY (`f1`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
START TRANSACTION;
INSERT INTO t1(f1) VALUES (1), (2), (3), (4), (5);
SELECT * FROM t1;
f1
1
2
3
4
5
COMMIT;
# Ensure that dirty pages of table t1 is flushed.
FLUSH TABLES t1 FOR EXPORT;
UNLOCK TABLES;
SET SESSION innodb_interpreter = 'init';
SET SESSION innodb_interpreter = 'open_table test/t1';
SET SESSION innodb_interpreter = 'find_space_id test/t1';
SET SESSION innodb_interpreter = 'find_flush_sync_lsn';
SELECT @@session.innodb_interpreter_output INTO @lsn;
SET SESSION innodb_interpreter = 'corrupt_ondisk_page0 test/t1';
SET SESSION innodb_interpreter = 'destroy';
DROP TABLE t1;
SET @@global.innodb_interpreter = @start_global_value;
SELECT @@global.innodb_interpreter;
@@global.innodb_interpreter
init
#
# Bug #33316661: setting innodb_interpreter=null terminate called after
# throwing an instance of 'std::logic_error'
#
SET SESSION innodb_interpreter = null;
ERROR 42000: Variable 'innodb_interpreter' can't be set to the value of 'NULL'
SET SESSION innodb_interpreter = 0;
ERROR 42000: Incorrect argument type to variable 'innodb_interpreter'
SET GLOBAL innodb_interpreter = null;
ERROR 42000: Variable 'innodb_interpreter' can't be set to the value of 'NULL'
SET GLOBAL innodb_interpreter = 0;
ERROR 42000: Incorrect argument type to variable 'innodb_interpreter'
