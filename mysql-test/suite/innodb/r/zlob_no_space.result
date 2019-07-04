#
# Bug #27852003 LOG::ALLOC_LOB_PAGE MAY FAIL AND RETURN NULLPTR
# BUT IT IS NOT HANDLED
#
SET GLOBAL innodb_compression_level = 0;
CREATE TABLE t1 (f1 INT PRIMARY KEY, f2 LONGBLOB) ROW_FORMAT=compressed;
SET DEBUG = '+d,innodb_alloc_lob_page_failed';
INSERT INTO t1 VALUES (1, REPEAT('x', 128*1024+1));
ERROR HY000: The table 't1' is full
SET DEBUG = '-d,innodb_alloc_lob_page_failed';
CREATE TABLE t2 (f1 INT PRIMARY KEY, f2 LONGBLOB) ROW_FORMAT=compressed;
SET DEBUG = '+d,innodb_lob_alloc_z_data_page_failed';
INSERT INTO t2 VALUES (1, REPEAT('x', 128*1024+1));
ERROR HY000: The table 't2' is full
SET DEBUG = '-d,innodb_lob_alloc_z_data_page_failed';
CREATE TABLE t3 (f1 INT PRIMARY KEY, f2 LONGBLOB) ROW_FORMAT=compressed;
SET DEBUG = '+d,innodb_lob_alloc_z_frag_page_failed';
INSERT INTO t3 VALUES (1, repeat('x', 128*1024+1));
ERROR HY000: The table 't3' is full
SET DEBUG = '-d,innodb_lob_alloc_z_frag_page_failed';
CREATE TABLE t4 (f1 INT PRIMARY KEY, f2 LONGBLOB) ROW_FORMAT=compressed;
SET DEBUG = '+d,innodb_lob_alloc_z_index_page_failed';
INSERT INTO t4 VALUES (1, REPEAT('x', 14417920));
ERROR HY000: The table 't4' is full
SET DEBUG = '-d,innodb_lob_alloc_z_index_page_failed';
CREATE TABLE t5 (f1 INT PRIMARY KEY, f2 LONGBLOB) ROW_FORMAT=compressed;
SET DEBUG = '+d,innodb_zlob_first_use_only_1_frag_entries';
SET DEBUG = '+d,innodb_lob_alloc_z_frag_node_page_failed';
INSERT INTO t5 VALUES (1, REPEAT('x', 28000000));
ERROR HY000: The table 't5' is full
SET DEBUG = '-d,innodb_lob_alloc_z_frag_node_page_failed';
SET DEBUG = '-d,innodb_zlob_first_use_only_1_frag_entries';
FLUSH TABLES t1, t2, t3, t4, t5 FOR EXPORT;
UNLOCK TABLES;
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;
DROP TABLE t4;
DROP TABLE t5;
SET GLOBAL innodb_compression_level = default;
