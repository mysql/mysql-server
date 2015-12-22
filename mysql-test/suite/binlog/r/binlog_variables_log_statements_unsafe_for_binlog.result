# restart
CALL mtr.add_suppression("Unsafe statement written to the binary log");
CREATE TABLE test.unsafe (i int auto_increment primary key, j int) engine = innodb;
INSERT INTO test.unsafe (j) VALUES (1), (2), (3);
SCENARIO 1
SET GLOBAL log_statements_unsafe_for_binlog= ON;
UPDATE test.unsafe SET j = 4 LIMIT 1;
Warnings:
Note	1592	Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT. The statement is unsafe because it uses a LIMIT clause. This is unsafe because the set of rows included cannot be predicted.
include/assert_grep.inc [Exactly one row should be written to the error log.]
SCENARIO 2
SET GLOBAL log_statements_unsafe_for_binlog= OFF;
UPDATE test.unsafe SET j = 4 LIMIT 1;
Warnings:
Note	1592	Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT. The statement is unsafe because it uses a LIMIT clause. This is unsafe because the set of rows included cannot be predicted.
DROP TABLE test.unsafe;
SET GLOBAL log_statements_unsafe_for_binlog= 1;
