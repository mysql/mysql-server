DROP DATABASE IF EXISTS test_jfg2;
Warnings:
Note	1008	Can't drop database 'test_jfg2'; database doesn't exist
DROP DATABASE IF EXISTS test_jfg;
Warnings:
Note	1008	Can't drop database 'test_jfg'; database doesn't exist
CREATE DATABASE test_jfg;
CREATE DATABASE test_jfg2;
CREATE TABLE test_jfg.test (a int unsigned NOT NULL PRIMARY KEY);
RENAME TABLE test_jfg.test TO test_jfg2.test;
SELECT * FROM INFORMATION_SCHEMA.INNODB_DATAFILES  WHERE PATH LIKE "%test%";
SPACE	PATH
#	./mtr/asserted_test_suppressions.ibd
#	./mtr/test_ignored_global_suppressions.ibd
#	./mtr/test_suppressions.ibd
#	./test_jfg2/test.ibd
DROP DATABASE test_jfg;
# restart
DROP DATABASE test_jfg2;
CREATE DATABASE abc_def;
CREATE DATABASE abc_def2;
CREATE TABLE abc_def.test (a int unsigned NOT NULL PRIMARY KEY);
RENAME TABLE abc_def.test TO abc_def2.test1;
SELECT * FROM INFORMATION_SCHEMA.INNODB_DATAFILES WHERE PATH LIKE "%test%";
SPACE	PATH
#	./abc_def2/test1.ibd
#	./mtr/asserted_test_suppressions.ibd
#	./mtr/test_ignored_global_suppressions.ibd
#	./mtr/test_suppressions.ibd
DROP DATABASE abc_def;
# restart
DROP DATABASE abc_def2;
#
# Bug #31773368 AUXILIARY TABLES FOR ALL FTS INDEX
# FOR A TABLE WITHOUT RENAMING
#
CREATE DATABASE test1;
CREATE TABLE t1(a int, t1_new_col varchar(100));
ALTER TABLE t1 ADD fulltext index full_t2_idx(t1_new_col);
Warnings:
Warning	124	InnoDB rebuilding table to add column FTS_DOC_ID
ALTER TABLE t1 DROP index full_t2_idx;
"List files under test"
fts_tableId_being_deleted.ibd
fts_tableId_being_deleted_cache.ibd
fts_tableId_config.ibd
fts_tableId_deleted.ibd
fts_tableId_deleted_cache.ibd
t1.ibd
RENAME TABLE t1 TO test1.t2;
"List files under test"
"List files under test1"
fts_tableId_being_deleted.ibd
fts_tableId_being_deleted_cache.ibd
fts_tableId_config.ibd
fts_tableId_deleted.ibd
fts_tableId_deleted_cache.ibd
t2.ibd
DROP TABLE test1.t2;
DROP DATABASE test1;
