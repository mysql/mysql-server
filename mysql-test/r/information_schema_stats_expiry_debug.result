SET SESSION debug= '+d,skip_dd_table_access_check';
CREATE TABLE t1(a int,KEY(a));
SELECT TABLE_NAME, ENGINE, ROW_FORMAT, TABLE_ROWS, AVG_ROW_LENGTH, DATA_LENGTH,
MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE, AUTO_INCREMENT from
information_schema.tables WHERE table_name='t1';
TABLE_NAME	ENGINE	ROW_FORMAT	TABLE_ROWS	AVG_ROW_LENGTH	DATA_LENGTH	MAX_DATA_LENGTH	INDEX_LENGTH	DATA_FREE	AUTO_INCREMENT
t1	InnoDB	Dynamic	0	0	16384	0	16384	0	NULL
SELECT table_name, table_rows, avg_row_length, data_length, max_data_length,
index_length, data_free, auto_increment from mysql.table_stats where table_name='t1';
table_name	table_rows	avg_row_length	data_length	max_data_length	index_length	data_free	auto_increment
t1	0	0	16384	0	16384	0	0
SELECT TABLE_NAME,COLUMN_NAME, INDEX_NAME, CARDINALITY from
information_schema.statistics where table_name='t1' ORDER BY COLUMN_NAME;
TABLE_NAME	COLUMN_NAME	INDEX_NAME	CARDINALITY
t1	a	a	0
SELECT table_name, column_name, index_name, cardinality from mysql.index_stats
where table_name='t1' ORDER bY column_name;
table_name	column_name	index_name	cardinality
t1	a	a	0
SET SESSION information_schema_stats_expiry=1;
INSERT INTO t1 VALUES(3);
SELECT SLEEP(2);
SLEEP(2)
0
SELECT TABLE_NAME, ENGINE, ROW_FORMAT, TABLE_ROWS, AVG_ROW_LENGTH, DATA_LENGTH,
MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE, AUTO_INCREMENT from
information_schema.tables WHERE table_name='t1';
TABLE_NAME	ENGINE	ROW_FORMAT	TABLE_ROWS	AVG_ROW_LENGTH	DATA_LENGTH	MAX_DATA_LENGTH	INDEX_LENGTH	DATA_FREE	AUTO_INCREMENT
t1	InnoDB	Dynamic	1	16384	16384	0	16384	0	NULL
SELECT table_name, table_rows, avg_row_length, data_length, max_data_length,
index_length, data_free, auto_increment from mysql.table_stats where table_name='t1';
table_name	table_rows	avg_row_length	data_length	max_data_length	index_length	data_free	auto_increment
t1	1	16384	16384	0	16384	0	0
SELECT TABLE_NAME,COLUMN_NAME, INDEX_NAME, CARDINALITY from
information_schema.statistics where table_name='t1' ORDER BY COLUMN_NAME;
TABLE_NAME	COLUMN_NAME	INDEX_NAME	CARDINALITY
t1	a	a	1
SELECT table_name, column_name, index_name, cardinality from mysql.index_stats
where table_name='t1' ORDER BY column_name;
table_name	column_name	index_name	cardinality
t1	a	a	1
DELETE FROM mysql.table_stats;
DELETE FROM mysql.index_stats;
DROP TABLE t1;
CREATE TABLE t2(a int,KEY(a));
CREATE TABLE t3(b int, KEY(b));
SET SESSION information_schema_stats_expiry=0;
SELECT TABLE_NAME, ENGINE, ROW_FORMAT, TABLE_ROWS, AVG_ROW_LENGTH, DATA_LENGTH,
MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE, AUTO_INCREMENT from
information_schema.tables WHERE table_name='t2';
TABLE_NAME	ENGINE	ROW_FORMAT	TABLE_ROWS	AVG_ROW_LENGTH	DATA_LENGTH	MAX_DATA_LENGTH	INDEX_LENGTH	DATA_FREE	AUTO_INCREMENT
t2	InnoDB	Dynamic	0	0	16384	0	16384	0	NULL
SELECT table_name, table_rows, avg_row_length, data_length, max_data_length,
index_length, data_free, auto_increment from mysql.table_stats where table_name='t2';
table_name	table_rows	avg_row_length	data_length	max_data_length	index_length	data_free	auto_increment
SET SESSION information_schema_stats_expiry=default;
START TRANSACTION;
INSERT INTO t2 VALUES(1);
SELECT TABLE_NAME, ENGINE, ROW_FORMAT, TABLE_ROWS, AVG_ROW_LENGTH, DATA_LENGTH,
MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE, AUTO_INCREMENT from
information_schema.tables WHERE table_name='t2';
TABLE_NAME	ENGINE	ROW_FORMAT	TABLE_ROWS	AVG_ROW_LENGTH	DATA_LENGTH	MAX_DATA_LENGTH	INDEX_LENGTH	DATA_FREE	AUTO_INCREMENT
t2	InnoDB	Dynamic	1	16384	16384	0	16384	0	NULL
ROLLBACK;
SELECT * FROM t2;
a
SELECT table_name, table_rows, avg_row_length, data_length, max_data_length,
index_length, data_free, auto_increment from mysql.table_stats where table_name='t2';
table_name	table_rows	avg_row_length	data_length	max_data_length	index_length	data_free	auto_increment
START TRANSACTION;
INSERT INTO t3 VALUES(1);
SELECT TABLE_NAME, ENGINE, ROW_FORMAT, TABLE_ROWS, AVG_ROW_LENGTH, DATA_LENGTH,
MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE, AUTO_INCREMENT from
information_schema.tables WHERE table_name='t2';
TABLE_NAME	ENGINE	ROW_FORMAT	TABLE_ROWS	AVG_ROW_LENGTH	DATA_LENGTH	MAX_DATA_LENGTH	INDEX_LENGTH	DATA_FREE	AUTO_INCREMENT
t2	InnoDB	Dynamic	1	16384	16384	0	16384	0	NULL
COMMIT;
SELECT * FROM t3;
b
1
SELECT TABLE_NAME, ENGINE, ROW_FORMAT, TABLE_ROWS, AVG_ROW_LENGTH, DATA_LENGTH,
MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE, AUTO_INCREMENT from
information_schema.tables WHERE table_name='t3';
TABLE_NAME	ENGINE	ROW_FORMAT	TABLE_ROWS	AVG_ROW_LENGTH	DATA_LENGTH	MAX_DATA_LENGTH	INDEX_LENGTH	DATA_FREE	AUTO_INCREMENT
t3	InnoDB	Dynamic	1	16384	16384	0	16384	0	NULL
DELETE FROM mysql.table_stats;
DELETE FROM mysql.index_stats;
DROP TABLE t2,t3;
SET SESSION information_schema_stats_expiry= default;
# Bug#32338335 STOP UPDATING I_S DYNAMIC STATS CACHE
# IN MULTI-STATEMENT TRANSACTIONS
CREATE TABLE t1 (a INT);
# CASE 1: Use START TRANSACTION
START TRANSACTION;
INSERT INTO t1 VALUES
(0),(1),(2),(3),(4),(5),(6),(7),(8),(9),
(10),(11),(12),(13),(14),(15),(16),(17),(18),(19);
# Make sure that we see latest dynamic statistics and do not cache it.
SELECT table_rows FROM information_schema.tables WHERE table_name = "t1";
TABLE_ROWS
20
COMMIT;
# Make sure that there is no dynamic statistics written
# by previous I_S query.
SELECT schema_name, table_name, table_rows
FROM mysql.table_stats WHERE table_name="t1";
schema_name	table_name	table_rows
# CASE 2: Use AUTOCOMMIT=0
SET AUTOCOMMIT=0;
INSERT INTO t1 VALUES
(0),(1),(2),(3),(4),(5),(6),(7),(8),(9),
(10),(11),(12),(13),(14),(15),(16),(17),(18),(19);
# Make sure that we see latest dynamic statistics and do not cache it.
SELECT table_rows FROM information_schema.tables WHERE table_name = "t1";
TABLE_ROWS
40
COMMIT;
SET AUTOCOMMIT=1;
# Make sure that there is no dynamic statistics written
# by previous I_S query.
SELECT schema_name, table_name, table_rows
FROM mysql.table_stats WHERE table_name="t1";
schema_name	table_name	table_rows
# Make sure that we see latest dynamic statistics and also cache it.
SELECT table_rows FROM information_schema.tables WHERE table_name = "t1";
TABLE_ROWS
40
# Make sure that there is dynamic statistics written
# by previous I_S query.
SELECT schema_name, table_name, table_rows
FROM mysql.table_stats WHERE table_name="t1";
schema_name	table_name	table_rows
test	t1	40
DROP table t1;
set global debug=RESET;
