select @@binlog_encryption;
@@binlog_encryption
1
select @@innodb_redo_log_encrypt;
@@innodb_redo_log_encrypt
1
select @@innodb_undo_log_encrypt;
@@innodb_undo_log_encrypt
1
create table t1 (f1 int, f2 blob) ENCRYPTION='Y';
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int DEFAULT NULL,
  `f2` blob
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ENCRYPTION='Y'
START TRANSACTION;
INSERT INTO t1 VALUES(1, repeat('#',12));
INSERT INTO t1 VALUES(2, repeat('+',12));
INSERT INTO t1 VALUES(3, repeat('/',12));
INSERT INTO t1 VALUES(4, repeat('-',12));
INSERT INTO t1 VALUES(5, repeat('.',12));
COMMIT WORK;
# Ensure that dirty pages of table t1 is flushed.
FLUSH TABLES t1 FOR EXPORT;
UNLOCK TABLES;
# Kill and restart
select 2;
2
2
SET SESSION innodb_interpreter = 'init';
SET SESSION innodb_interpreter = 'print_dblwr_has_encrypted_pages';
SET SESSION innodb_interpreter = 'destroy';
Pattern "Double write file has encrypted pages" found
Pattern "\[Note\] \[MY-\d+\] \[InnoDB\] Recovered page \[page id: space=\d+, page number=\d+\] from the doublewrite buffer" not found
DROP TABLE t1;
# Test End
