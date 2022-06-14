#
# WL#13775 - InnoDB: Encrypt DBLWR files
#
#
# Test Begin: Test if recovery works if first page of user
# tablespace is corrupted.  Valid copy of that page is there
# in DBLWR file.
#
SET SESSION innodb_interpreter = 'init';
CREATE TABLE t1 (f1 INT PRIMARY KEY, f2 BLOB) ENCRYPTION='Y';
START TRANSACTION;
INSERT INTO t1 VALUES(1, repeat('#',12));
INSERT INTO t1 VALUES(2, repeat('+',12));
INSERT INTO t1 VALUES(3, repeat('/',12));
COMMIT WORK;
SELECT space from INFORMATION_SCHEMA.INNODB_TABLESPACES
WHERE name = 'test/t1' INTO @space_id;
# Ensure that dirty pages of table t1 is flushed.
FLUSH TABLES t1 FOR EXPORT;
UNLOCK TABLES;
SET SESSION innodb_interpreter = 'find_tablespace_file_name test/t1';
SELECT @@session.innodb_interpreter_output INTO @space_file_name;
SET SESSION innodb_interpreter = 'find_tablespace_physical_page_size test/t1';
SELECT @@session.innodb_interpreter_output INTO @space_page_size;
START TRANSACTION;
INSERT INTO t1 VALUES(4, repeat('.',12));
SET SESSION innodb_interpreter = CONCAT('dblwr_force_crash ', @space_id, ' 0');
SET SESSION innodb_interpreter = CONCAT('make_page_dirty ', @space_id, ' 0');
ERROR HY000: Lost connection to MySQL server during query
# restart
Pattern "\[Note\] .* Restoring page \[page id: space=\d+, page number=0\] of datafile .* from the doublewrite buffer. Writing .*" found
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
SELECT f1, f2 FROM t1;
f1	f2
1	############
2	++++++++++++
3	////////////
DROP TABLE t1;
# Test End
