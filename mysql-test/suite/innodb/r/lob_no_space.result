#
# Bug #27852003 LOG::ALLOC_LOB_PAGE MAY FAIL AND RETURN NULLPTR
# BUT IT IS NOT HANDLED
#
CREATE TABLE t1 (f1 INT PRIMARY KEY, f2 LONGBLOB);
SET DEBUG = '+d,innodb_alloc_lob_page_failed';
INSERT INTO t1 VALUES (1, REPEAT('x', 81920));
ERROR HY000: The table 't1' is full
SET DEBUG = '-d,innodb_alloc_lob_page_failed';
CHECK TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	check	status	OK
CREATE TABLE t2 (f1 INT PRIMARY KEY, f2 LONGBLOB);
SET DEBUG = '+d,innodb_lob_data_page_alloc_failed';
INSERT INTO t2 VALUES (1, REPEAT('x', 81920));
ERROR HY000: The table 't2' is full
SET DEBUG = '-d,innodb_lob_data_page_alloc_failed';
CHECK TABLE t2;
Table	Op	Msg_type	Msg_text
test.t2	check	status	OK
CREATE TABLE t3 (f1 INT PRIMARY KEY, f2 LONGBLOB);
SET DEBUG = '+d,innodb_lob_alloc_node_page_failed';
INSERT INTO t3 VALUES (1, REPEAT('x', 163840));
ERROR HY000: The table 't3' is full
SET DEBUG = '-d,innodb_lob_alloc_node_page_failed';
CHECK TABLE t3;
Table	Op	Msg_type	Msg_text
test.t3	check	status	OK
FLUSH TABLES t1 FOR EXPORT;
UNLOCK TABLES;
FLUSH TABLES t2 FOR EXPORT;
UNLOCK TABLES;
FLUSH TABLES t3 FOR EXPORT;
UNLOCK TABLES;
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;
