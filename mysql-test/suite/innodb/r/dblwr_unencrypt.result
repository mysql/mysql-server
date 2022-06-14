#
# WL#13775 - InnoDB: Encrypt DBLWR files
#
#
# Test Begin: Test if recovery works if first page of user
# tablespace is corrupted.  Valid copy of that page is there
# in DBLWR file.
#
# Wait for purge to complete
CREATE TABLE t1 (f1 INT PRIMARY KEY, f2 BLOB);
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
SET GLOBAL innodb_master_thread_disabled_debug=1;
SET GLOBAL innodb_checkpoint_disabled=1;
BEGIN;
INSERT INTO t1 VALUES (6, repeat('%', 12));
SET SESSION innodb_interpreter = 'init';
SET SESSION innodb_interpreter = 'find_tablespace_file_name test/t1';
SELECT @@session.innodb_interpreter_output INTO @space_file_name;
SET SESSION innodb_interpreter = 'destroy';
# Kill the server
# Corrupt the root page of table t1 in the user tablespace.
# restart
SET SESSION innodb_interpreter = 'init';
SET SESSION innodb_interpreter = 'print_dblwr_has_encrypted_pages';
SET SESSION innodb_interpreter = 'destroy';
Pattern "Double write file has encrypted pages" not found
Pattern "\[Note\] .* Restoring page \[page id: space=\d+, page number=0\] of datafile .* from the doublewrite buffer. Writing .*" found
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
SELECT f1, f2 FROM t1;
f1	f2
1	############
2	++++++++++++
3	////////////
4	------------
5	............
DROP TABLE t1;
# Test End
