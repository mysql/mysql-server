#
# Bug#30591967  BACKUP FAILS WITH "CANNOT STORE RESULT" ERROR
# WHEN DDL EXECUTED IN PARALLEL
#
#
# Case 1 : Using non-partitioned table.
#
CREATE DATABASE db1;
CREATE TABLE db1.t1 (f1 INT) ENGINE='InnoDB';
SELECT * FROM INFORMATION_SCHEMA.INNODB_TABLES WHERE name like 'db1/t1';
TABLE_ID	NAME	FLAG	N_COLS	SPACE	ROW_FORMAT	ZIP_PAGE_SIZE	SPACE_TYPE	INSTANT_COLS	TOTAL_ROW_VERSIONS	INITIAL_COLUMN_COUNTS	CURRENT_COLUMN_COUNTS	TOTAL_COLUMN_COUNTS
#	db1/t1	33	4	#	Dynamic	0	Single	0	0	1	1	1
# Evict db1.t1 from innodb cache by restarting the server.
# restart
# Wait after acquiring table object, but before schema object is acquired.
SET GLOBAL DEBUG= '+d,before_acquire_schema_by_private_id';
SET DEBUG_SYNC="wait_before_acquire_schema_by_private_id SIGNAL cond1 WAIT_FOR cond2";
SELECT * FROM INFORMATION_SCHEMA.INNODB_TABLES WHERE name = 'db1/t1';;
# Drop the schema db1 in other session.
SET DEBUG_SYNC= 'now WAIT_FOR cond1';
DROP DATABASE db1;
SET DEBUG_SYNC= 'now SIGNAL cond2';
# Continue acquiring db1. reap results
# in a error ER_BAD_DB_ERROR without fix.
TABLE_ID	NAME	FLAG	N_COLS	SPACE	ROW_FORMAT	ZIP_PAGE_SIZE	SPACE_TYPE	INSTANT_COLS	TOTAL_ROW_VERSIONS	INITIAL_COLUMN_COUNTS	CURRENT_COLUMN_COUNTS	TOTAL_COLUMN_COUNTS
# Cleanup
SET DEBUG_SYNC= 'RESET';
SET GLOBAL DEBUG= DEFAULT;
#
# Case 2 : Using partitioned table.
#
CREATE DATABASE db1;
CREATE TABLE db1.t1 (a INT NOT NULL, b INT NOT NULL)
PARTITION BY KEY (a) PARTITIONS 1;
SELECT * FROM INFORMATION_SCHEMA.INNODB_TABLES WHERE name like 'db1/t1#p#p0';
TABLE_ID	NAME	FLAG	N_COLS	SPACE	ROW_FORMAT	ZIP_PAGE_SIZE	SPACE_TYPE	INSTANT_COLS	TOTAL_ROW_VERSIONS	INITIAL_COLUMN_COUNTS	CURRENT_COLUMN_COUNTS	TOTAL_COLUMN_COUNTS
#	db1/t1#p#p0	33	5	#	Dynamic	0	Single	0	0	2	2	2
# Evict db1.t1 from innodb cache by restarting the server.
# restart
# Wait after acquiring table object, but before schema object is acquired.
SET GLOBAL DEBUG= '+d,before_acquire_schema_by_private_id';
SET DEBUG_SYNC="wait_before_acquire_schema_by_private_id SIGNAL cond1 WAIT_FOR cond2";
SELECT * FROM INFORMATION_SCHEMA.INNODB_TABLES WHERE name = 'db1/t1#p#p0';;
# Drop the schema db1 in other session.
SET DEBUG_SYNC= 'now WAIT_FOR cond1';
DROP DATABASE db1;
SET DEBUG_SYNC= 'now SIGNAL cond2';
# Continue acquiring db1. reap results
# in a error ER_BAD_DB_ERROR without fix.
TABLE_ID	NAME	FLAG	N_COLS	SPACE	ROW_FORMAT	ZIP_PAGE_SIZE	SPACE_TYPE	INSTANT_COLS	TOTAL_ROW_VERSIONS	INITIAL_COLUMN_COUNTS	CURRENT_COLUMN_COUNTS	TOTAL_COLUMN_COUNTS
# Cleanup
SET DEBUG_SYNC= 'RESET';
SET GLOBAL DEBUG= DEFAULT;
