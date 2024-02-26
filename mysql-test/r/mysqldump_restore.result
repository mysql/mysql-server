# Set concurrent_insert = 0 to prevent random errors
# will reset to original value at the end of the test
SET @old_concurrent_insert = @@global.concurrent_insert;
SET @@global.concurrent_insert = 0;
# Pre-test cleanup
DROP TABLE IF EXISTS t1;
# Begin tests
#
# Bug#2005 Long decimal comparison bug.
#
CREATE TABLE t1 (a DECIMAL(64, 20));
INSERT INTO t1 VALUES ("1234567890123456789012345678901234567890"),
("0987654321098765432109876543210987654321");
# Begin testing mysqldump output + restore
# Create 'original table name - <table>_orig
SET @orig_table_name = CONCAT('test.t1', '_orig');
# Rename original table
ALTER TABLE test.t1 RENAME to test.t1_orig;
# Recreate table from mysqldump output
# Compare original and recreated tables
# Recreated table: test.t1
# Original table: test.t1_orig
include/diff_tables.inc [test.t1, test.t1_orig]
# Cleanup
DROP TABLE test.t1, test.t1_orig;
#
# Bug#3361 mysqldump quotes DECIMAL values inconsistently
#
CREATE TABLE t1 (a DECIMAL(10,5), b FLOAT);
INSERT INTO t1 VALUES (1.2345, 2.3456);
INSERT INTO t1 VALUES ('1.2345', 2.3456);
INSERT INTO t1 VALUES ("1.2345", 2.3456);
SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='ANSI_QUOTES';
INSERT INTO t1 VALUES (1.2345, 2.3456);
INSERT INTO t1 VALUES ('1.2345', 2.3456);
INSERT INTO t1 VALUES ("1.2345", 2.3456);
ERROR 42S22: Unknown column '1.2345' in 'field list'
SET SQL_MODE=@OLD_SQL_MODE;
# Begin testing mysqldump output + restore
# Create 'original table name - <table>_orig
SET @orig_table_name = CONCAT('test.t1', '_orig');
# Rename original table
ALTER TABLE test.t1 RENAME to test.t1_orig;
# Recreate table from mysqldump output
# Compare original and recreated tables
# Recreated table: test.t1
# Original table: test.t1_orig
include/diff_tables.inc [test.t1, test.t1_orig]
# Cleanup
DROP TABLE test.t1, test.t1_orig;
#
# Bug#1994 mysqldump does not correctly dump UCS2 data
# Bug#4261 mysqldump 10.7 (mysql 4.1.2) --skip-extended-insert drops NULL from inserts
#
CREATE TABLE t1 (a  VARCHAR(255)) DEFAULT CHARSET koi8r;
INSERT INTO t1  VALUES (_koi8r x'C1C2C3C4C5'), (NULL);
# Begin testing mysqldump output + restore
# Create 'original table name - <table>_orig
SET @orig_table_name = CONCAT('test.t1', '_orig');
# Rename original table
ALTER TABLE test.t1 RENAME to test.t1_orig;
# Recreate table from mysqldump output
# Compare original and recreated tables
# Recreated table: test.t1
# Original table: test.t1_orig
include/diff_tables.inc [test.t1, test.t1_orig]
# Cleanup
DROP TABLE test.t1, test.t1_orig;
#
# WL#2319 Exclude Tables from dump
#
CREATE TABLE t1 (a INT);
CREATE TABLE t2 (a INT);
INSERT INTO t1 VALUES (1),(2),(3);
INSERT INTO t2 VALUES (4),(5),(6);
# Begin testing mysqldump output + restore
# Create 'original table name - <table>_orig
SET @orig_table_name = CONCAT('test.t2', '_orig');
# Rename original table
ALTER TABLE test.t2 RENAME to test.t2_orig;
# Recreate table from mysqldump output
# Compare original and recreated tables
# Recreated table: test.t2
# Original table: test.t2_orig
include/diff_tables.inc [test.t2, test.t2_orig]
# Cleanup
DROP TABLE test.t2, test.t2_orig;
DROP TABLE t1;
#
# Bug#8830 mysqldump --skip-extended-insert causes --hex-blob to dump wrong values
#
CREATE TABLE t1 (`b` blob);
INSERT INTO `t1` VALUES (0x602010000280100005E71A);
# Begin testing mysqldump output + restore
# Create 'original table name - <table>_orig
SET @orig_table_name = CONCAT('test.t1', '_orig');
# Rename original table
ALTER TABLE test.t1 RENAME to test.t1_orig;
# Recreate table from mysqldump output
# Compare original and recreated tables
# Recreated table: test.t1
# Original table: test.t1_orig
include/diff_tables.inc [test.t1, test.t1_orig]
# Cleanup
DROP TABLE test.t1, test.t1_orig;
# End tests
# Cleanup
# Reset concurrent_insert to its original value
SET @@global.concurrent_insert = @old_concurrent_insert;
# remove mysqldumpfile
