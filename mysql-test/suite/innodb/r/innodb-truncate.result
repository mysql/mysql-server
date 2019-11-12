#
# TRUNCATE TABLE
#
# Truncating is disallowed for parent tables unless such table
# participates in self-referencing foreign keys only.
#
CREATE TABLE t1 (pk INT PRIMARY KEY) ENGINE=INNODB;
CREATE TABLE t2 (fk INT NOT NULL, FOREIGN KEY (fk) REFERENCES t1 (pk)) ENGINE=INNODB;
TRUNCATE TABLE t1;
ERROR 42000: Cannot truncate a table referenced in a foreign key constraint (`test`.`t2`, CONSTRAINT `t2_ibfk_1`)
# Truncation of child should succeed.
TRUNCATE TABLE t2;
DROP TABLE t2;
DROP TABLE t1;
CREATE TABLE t1 (pk INT PRIMARY KEY, fk INT,
FOREIGN KEY (fk) REFERENCES t1 (pk)) ENGINE=INNODB;
# Truncation of self-referencing table should succeed.
TRUNCATE TABLE t1;
DROP TABLE t1;
#
# Also, truncating such tables is allowed if foreign key
# checks are disabled.
#
SET @old_foreign_key_checks = @@SESSION.foreign_key_checks;
CREATE TABLE t1 (pk INT PRIMARY KEY) ENGINE=INNODB;
CREATE TABLE t2 (fk INT NOT NULL, FOREIGN KEY (fk) REFERENCES t1 (pk)) ENGINE=INNODB;
CREATE TABLE t3 (pk INT PRIMARY KEY, fk INT,
FOREIGN KEY (fk) REFERENCES t1 (pk)) ENGINE=INNODB;
SET @@SESSION.foreign_key_checks = 0;
TRUNCATE TABLE t1;
TRUNCATE TABLE t2;
TRUNCATE TABLE t3;
SET @@SESSION.foreign_key_checks = 1;
TRUNCATE TABLE t1;
ERROR 42000: Cannot truncate a table referenced in a foreign key constraint (`test`.`t2`, CONSTRAINT `t2_ibfk_1`)
TRUNCATE TABLE t2;
TRUNCATE TABLE t3;
LOCK TABLES t1 WRITE;
SET @@SESSION.foreign_key_checks = 0;
TRUNCATE TABLE t1;
SET @@SESSION.foreign_key_checks = 1;
TRUNCATE TABLE t1;
ERROR 42000: Cannot truncate a table referenced in a foreign key constraint (`test`.`t2`, CONSTRAINT `t2_ibfk_1`)
UNLOCK TABLES;
DROP TABLE t3,t2,t1;
SET @@SESSION.foreign_key_checks = @old_foreign_key_checks;
#
# Test that TRUNCATE resets auto-increment.
#
CREATE TABLE t1 (a INT PRIMARY KEY NOT NULL AUTO_INCREMENT);
INSERT INTO t1 VALUES (NULL), (NULL);
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES WHERE table_name = 't1';
AUTO_INCREMENT
3
SELECT * FROM t1 ORDER BY a;
a
1
2
TRUNCATE TABLE t1;
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES WHERE table_name = 't1';
AUTO_INCREMENT
1
INSERT INTO t1 VALUES (NULL), (NULL);
SELECT * FROM t1 ORDER BY a;
a
1
2
DROP TABLE t1;
#
# Test that TRUNCATE with innodb_default_row_format changed
#
CREATE TEMPORARY TABLE t1(c1 DOUBLE(1,0)ZEROFILL,c2 BLOB,c3 YEAR)
ENGINE=InnoDB;
Warnings:
Warning	1681	The ZEROFILL attribute is deprecated and will be removed in a future release. Use the LPAD function to zero-pad numbers, or store the formatted numbers in a CHAR column.
Warning	1681	Specifying number of digits for floating point data types is deprecated and will be removed in a future release.
ALTER TABLE t1 MODIFY c2 VARCHAR(1024)BINARY CHARACTER SET 'Binary' COLLATE
'Binary';
Warnings:
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
ALTER TABLE t1 ADD INDEX(c2),ADD UNIQUE (c2);
SET @@GLOBAL.innodb_default_row_format=1;
TRUNCATE t1;
DROP TABLE t1;
SET @@GLOBAL.innodb_default_row_format=default;
CREATE  TABLE t1(c1 DOUBLE(1,0)ZEROFILL,c2 BLOB,c3 YEAR)
ENGINE=InnoDB;
Warnings:
Warning	1681	The ZEROFILL attribute is deprecated and will be removed in a future release. Use the LPAD function to zero-pad numbers, or store the formatted numbers in a CHAR column.
Warning	1681	Specifying number of digits for floating point data types is deprecated and will be removed in a future release.
ALTER TABLE t1 MODIFY c2 VARCHAR(1024)BINARY CHARACTER SET 'Binary' COLLATE
'Binary';
Warnings:
Warning	1287	'BINARY as attribute of a type' is deprecated and will be removed in a future release. Please use a CHARACTER SET clause with _bin collation instead
ALTER TABLE t1 ADD INDEX(c2),ADD UNIQUE (c2);
alter table t1 partition by key (c2) partitions 2;
SET @@GLOBAL.innodb_default_row_format=1;
TRUNCATE t1;
DROP TABLE t1;
SET @@GLOBAL.innodb_default_row_format=default;
