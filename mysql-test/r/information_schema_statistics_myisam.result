CREATE TABLE t1 (f0 INT KEY AUTO_INCREMENT, f1 INT, f2 INT);
CREATE TABLE t2 (f0 INT KEY AUTO_INCREMENT, f1 INT, f2 INT) ENGINE=MYISAM;
CREATE TABLE t3 (f0 INT KEY AUTO_INCREMENT, f1 INT, f2 INT, KEY (f0), KEY (f1), KEY (f1, f2));
INSERT INTO t1 (f1, f2) VALUES (1,1 ),(2,2),(3,3),(4,4);
INSERT INTO t1 (f1, f2) SELECT f1, f2 FROM t1;
INSERT INTO t1 (f1, f2) SELECT f1, f2 FROM t1;
INSERT INTO t1 (f1, f2) SELECT f1, f2 FROM t1;
INSERT INTO t2 (f1, f2) SELECT f1, f2 FROM t1;
INSERT INTO t3 (f1, f2) SELECT f1, f2 FROM t1;
DELETE FROM t1 WHERE f1=1;
DELETE FROM t2 WHERE f1=1;
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
ANALYZE TABLE t2;
Table	Op	Msg_type	Msg_text
test.t2	analyze	status	OK
ANALYZE TABLE t3;
Table	Op	Msg_type	Msg_text
test.t3	analyze	status	Table is already up to date
SELECT COUNT(*) FROM t1;
COUNT(*)
24
SELECT COUNT(*) FROM t2;
COUNT(*)
24
SELECT COUNT(*) FROM t3;
COUNT(*)
32
Case 1: IS query uses mysql.table_stats to read dynamic table statistics
SET SESSION information_schema_stats_expiry=default;
SELECT table_rows, avg_row_length, data_length, max_data_length, index_length,
data_free, auto_increment, checksum, update_time, check_time
FROM information_schema.tables WHERE table_name in ('t1', 't2');
TABLE_ROWS	AVG_ROW_LENGTH	DATA_LENGTH	MAX_DATA_LENGTH	INDEX_LENGTH	DATA_FREE	AUTO_INCREMENT	CHECKSUM	UPDATE_TIME	CHECK_TIME
24	13	416	3659174697238527	2048	104	33	NULL	#	NULL
24	13	416	3659174697238527	2048	104	33	NULL	#	NULL
SELECT * FROM information_schema.statistics WHERE table_name='t3'
ORDER BY index_name, seq_in_index;
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	NON_UNIQUE	INDEX_SCHEMA	INDEX_NAME	SEQ_IN_INDEX	COLUMN_NAME	COLLATION	CARDINALITY	SUB_PART	PACKED	NULLABLE	INDEX_TYPE	COMMENT	INDEX_COMMENT	IS_VISIBLE	EXPRESSION
def	test	t3	1	test	f0	1	f0	A	32	NULL	NULL		BTREE			YES	NULL
def	test	t3	1	test	f1	1	f1	A	4	NULL	NULL	YES	BTREE			YES	NULL
def	test	t3	1	test	f1_2	1	f1	A	4	NULL	NULL	YES	BTREE			YES	NULL
def	test	t3	1	test	f1_2	2	f2	A	4	NULL	NULL	YES	BTREE			YES	NULL
def	test	t3	0	test	PRIMARY	1	f0	A	32	NULL	NULL		BTREE			YES	NULL
Case 2: IS query uses UDF's to read dynamic table statistics
SET SESSION information_schema_stats_expiry=0;
SELECT table_rows, avg_row_length, data_length, max_data_length, index_length,
data_free, auto_increment, checksum, update_time, check_time
FROM information_schema.tables WHERE table_name in ('t1', 't2');
TABLE_ROWS	AVG_ROW_LENGTH	DATA_LENGTH	MAX_DATA_LENGTH	INDEX_LENGTH	DATA_FREE	AUTO_INCREMENT	CHECKSUM	UPDATE_TIME	CHECK_TIME
24	13	416	3659174697238527	2048	104	33	NULL	#	NULL
24	13	416	3659174697238527	2048	104	33	NULL	#	NULL
SELECT * FROM information_schema.statistics WHERE table_name='t3'
ORDER BY index_name, seq_in_index;
TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	NON_UNIQUE	INDEX_SCHEMA	INDEX_NAME	SEQ_IN_INDEX	COLUMN_NAME	COLLATION	CARDINALITY	SUB_PART	PACKED	NULLABLE	INDEX_TYPE	COMMENT	INDEX_COMMENT	IS_VISIBLE	EXPRESSION
def	test	t3	1	test	f0	1	f0	A	32	NULL	NULL		BTREE			YES	NULL
def	test	t3	1	test	f1	1	f1	A	4	NULL	NULL	YES	BTREE			YES	NULL
def	test	t3	1	test	f1_2	1	f1	A	4	NULL	NULL	YES	BTREE			YES	NULL
def	test	t3	1	test	f1_2	2	f2	A	4	NULL	NULL	YES	BTREE			YES	NULL
def	test	t3	0	test	PRIMARY	1	f0	A	32	NULL	NULL		BTREE			YES	NULL
SET SESSION information_schema_stats_expiry=default;
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;
