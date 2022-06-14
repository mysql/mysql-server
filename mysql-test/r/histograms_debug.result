# Testing of histogram statistics that uses DEBUG functionality.
#
# Simulate a failure due to dropping histograms during DROP TABLE
#
CREATE TABLE t1 (col1 INT, col2 INT);
ANALYZE TABLE t1 UPDATE HISTOGRAM ON col1, col2 WITH 10 BUCKETS;
Table	Op	Msg_type	Msg_text
test.t1	histogram	status	Histogram statistics created for column 'col1'.
test.t1	histogram	status	Histogram statistics created for column 'col2'.
SELECT schema_name, table_name, column_name
FROM information_schema.COLUMN_STATISTICS;
SCHEMA_NAME	TABLE_NAME	COLUMN_NAME
test	t1	col1
test	t1	col2
SELECT schema_name, table_name, column_name
FROM information_schema.COLUMN_STATISTICS;
SCHEMA_NAME	TABLE_NAME	COLUMN_NAME
test	t1	col1
test	t1	col2
SELECT COUNT(*) FROM information_schema.TABLES
WHERE TABLE_SCHEMA = 'test' AND TABLE_NAME = 't1';
COUNT(*)
1
SET DEBUG='+d,fail_after_drop_histograms';
DROP TABLE t1;
ERROR HY000: Unable to remove column statistics for column 'dummy_column' in table 'test'.'t1'
SELECT schema_name, table_name, column_name
FROM information_schema.COLUMN_STATISTICS;
SCHEMA_NAME	TABLE_NAME	COLUMN_NAME
test	t1	col1
test	t1	col2
SELECT COUNT(*) FROM information_schema.TABLES
WHERE TABLE_SCHEMA = 'test' AND TABLE_NAME = 't1';
COUNT(*)
1
SET DEBUG='-d,fail_after_drop_histograms';
#
# Simulate a failure due to dropping histograms during ALTER TABLE
#
SELECT schema_name, table_name, column_name
FROM information_schema.COLUMN_STATISTICS;
SCHEMA_NAME	TABLE_NAME	COLUMN_NAME
test	t1	col1
test	t1	col2
SELECT COUNT(*) FROM information_schema.COLUMNS
WHERE TABLE_SCHEMA = 'test' AND TABLE_NAME = 't1' AND COLUMN_NAME = 'col2';
COUNT(*)
1
SET DEBUG='+d,fail_after_drop_histograms';
ALTER TABLE t1 DROP COLUMN col2;
ERROR HY000: Unable to remove column statistics for column 'dummy_column' in table 'test'.'t1'
SELECT schema_name, table_name, column_name
FROM information_schema.COLUMN_STATISTICS;
SCHEMA_NAME	TABLE_NAME	COLUMN_NAME
test	t1	col1
test	t1	col2
SELECT COUNT(*) FROM information_schema.COLUMNS
WHERE TABLE_SCHEMA = 'test' AND TABLE_NAME = 't1' AND COLUMN_NAME = 'col2';
COUNT(*)
1
SET DEBUG='-d,fail_after_drop_histograms';
#
# Simulate a failure due to renaming histograms during ALTER TABLE RENAME
#
SELECT schema_name, table_name, column_name
FROM information_schema.COLUMN_STATISTICS;
SCHEMA_NAME	TABLE_NAME	COLUMN_NAME
test	t1	col1
test	t1	col2
SELECT COUNT(*) FROM information_schema.TABLES
WHERE TABLE_SCHEMA = 'test' AND TABLE_NAME = 't1';
COUNT(*)
1
SET DEBUG='+d,fail_after_rename_histograms';
ALTER TABLE t1 RENAME TO t2;
ERROR HY000: Unable to update column statistics for column 'dummy_column' in table 'test'.'t1'
SELECT schema_name, table_name, column_name
FROM information_schema.COLUMN_STATISTICS;
SCHEMA_NAME	TABLE_NAME	COLUMN_NAME
test	t1	col1
test	t1	col2
SELECT COUNT(*) FROM information_schema.TABLES
WHERE TABLE_SCHEMA = 'test' AND TABLE_NAME = 't1';
COUNT(*)
1
SET DEBUG='-d,fail_after_rename_histograms';
DROP TABLE t1;
#
# Check that histogram with sampling works as expected
#
SET DEBUG='+d,histogram_force_sampling';
CREATE TABLE t1 (col1 DOUBLE);
INSERT INTO t1 SELECT RAND(1);
INSERT INTO t1 SELECT RAND(2) FROM t1;
INSERT INTO t1 SELECT RAND(3) FROM t1;
INSERT INTO t1 SELECT RAND(4) FROM t1;
INSERT INTO t1 SELECT RAND(5) FROM t1;
INSERT INTO t1 SELECT RAND(6) FROM t1;
INSERT INTO t1 SELECT RAND(7) FROM t1;
INSERT INTO t1 SELECT RAND(8) FROM t1;
INSERT INTO t1 SELECT RAND(9) FROM t1;
INSERT INTO t1 SELECT RAND(10) FROM t1;
ANALYZE TABLE t1 UPDATE HISTOGRAM ON col1 WITH 4 BUCKETS;
Table	Op	Msg_type	Msg_text
test.t1	histogram	status	Histogram statistics created for column 'col1'.
SELECT schema_name, table_name, column_name,
JSON_REMOVE(histogram, '$."last-updated"')
FROM information_schema.COLUMN_STATISTICS;
SCHEMA_NAME	TABLE_NAME	COLUMN_NAME	JSON_REMOVE(histogram, '$."last-updated"')
test	t1	col1	{"buckets": [], "data-type": "double", "null-values": 0.0, "collation-id": 8, "sampling-rate": 0.5, "histogram-type": "singleton", "number-of-buckets-specified": 4}
SET DEBUG='-d,histogram_force_sampling';
DROP TABLE t1;
#
# Bug#26020352 WL8943:ASSERTION `M_THD->GET_TRANSACTION()->IS_EMPTY(
#              TRANSACTION_CTX::STMT) && M
#
CREATE TABLE t1 (col1 INT);
ANALYZE TABLE t1 UPDATE HISTOGRAM ON col1 WITH 10 BUCKETS;
Table	Op	Msg_type	Msg_text
test.t1	histogram	status	Histogram statistics created for column 'col1'.
SELECT schema_name, table_name, column_name
FROM information_schema.COLUMN_STATISTICS;
SCHEMA_NAME	TABLE_NAME	COLUMN_NAME
test	t1	col1
SET DEBUG='+d,histogram_fail_after_open_table';
ANALYZE TABLE t1 UPDATE HISTOGRAM ON col1 WITH 10 BUCKETS;
Table	Op	Msg_type	Msg_text
SELECT schema_name, table_name, column_name
FROM information_schema.COLUMN_STATISTICS;
SCHEMA_NAME	TABLE_NAME	COLUMN_NAME
test	t1	col1
SET DEBUG='-d,histogram_fail_after_open_table';
DROP TABLE t1;
#
# Bug#26027240 WL8943:VIRTUAL BOOL SQL_CMD_ANALYZE_TABLE::EXECUTE(THD*):
#              ASSERTION `FALSE' FAIL
#
CREATE TABLE t1 (col1 INT);
ANALYZE TABLE t1 UPDATE HISTOGRAM ON col1 WITH 10 BUCKETS;
Table	Op	Msg_type	Msg_text
test.t1	histogram	status	Histogram statistics created for column 'col1'.
SET DEBUG='+d,histogram_fail_during_lock_for_write';
ANALYZE TABLE t1 DROP HISTOGRAM ON col1;
Table	Op	Msg_type	Msg_text
# Since we have simulated a fail, the histogram should still be present.
# However, since this is a simulation of failure no error is reported.
SELECT schema_name, table_name, column_name
FROM information_schema.COLUMN_STATISTICS;
SCHEMA_NAME	TABLE_NAME	COLUMN_NAME
test	t1	col1
SET DEBUG='-d,histogram_fail_during_lock_for_write';
ANALYZE TABLE t1 DROP HISTOGRAM ON col1;
Table	Op	Msg_type	Msg_text
test.t1	histogram	status	Histogram statistics removed for column 'col1'.
# The histogram should now be gone.
SELECT schema_name, table_name, column_name
FROM information_schema.COLUMN_STATISTICS;
SCHEMA_NAME	TABLE_NAME	COLUMN_NAME
DROP TABLE t1;
#
# Bug#26772858 MDL FOR COLUMN STATISTICS IS NOT PROPERLY REFLECTED IN
# P_S.METADATA_LOCKS
#
CREATE TABLE t1 (col1 INT);
SET DEBUG_SYNC='store_histogram_after_write_lock SIGNAL histogram_1_waiting WAIT_FOR continue_store_histogram';
ANALYZE TABLE t1 UPDATE HISTOGRAM ON col1 WITH 2 BUCKETS;;
SET DEBUG_SYNC='now WAIT_FOR histogram_1_waiting';
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, COLUMN_NAME
FROM performance_schema.metadata_locks
WHERE LOCK_TYPE = "EXCLUSIVE"
    AND OBJECT_TYPE = "COLUMN STATISTICS"
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, COLUMN_NAME;
OBJECT_TYPE	OBJECT_SCHEMA	OBJECT_NAME	COLUMN_NAME
COLUMN STATISTICS	test	t1	col1
SET DEBUG_SYNC='mdl_acquire_lock_wait SIGNAL histogram_2_lock_waiting';
ANALYZE TABLE t1 UPDATE HISTOGRAM ON col1 WITH 2 BUCKETS;;
SET DEBUG_SYNC='now WAIT_FOR histogram_2_lock_waiting';
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, INDEX_NAME
FROM performance_schema.events_waits_current
WHERE OBJECT_TYPE = "COLUMN STATISTICS"
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, INDEX_NAME;
OBJECT_TYPE	OBJECT_SCHEMA	OBJECT_NAME	INDEX_NAME
COLUMN STATISTICS	test	t1	col1
SET DEBUG_SYNC='now SIGNAL continue_store_histogram';
Table	Op	Msg_type	Msg_text
test.t1	histogram	status	Histogram statistics created for column 'col1'.
Table	Op	Msg_type	Msg_text
test.t1	histogram	status	Histogram statistics created for column 'col1'.
DROP TABLE t1;
#
# Bug#27672693  HISTOGRAMS: ASSERTION FAILED: !THD->TX_READ_ONLY
#
CREATE TABLE t1(col1 INT);
SET LOCAL TRANSACTION READ ONLY;
INSERT INTO t1 (col1) VALUES (1);
ERROR 25006: Cannot execute statement in a READ ONLY transaction.
ANALYZE TABLE t1 UPDATE HISTOGRAM ON col1 WITH 16 BUCKETS;
Table	Op	Msg_type	Msg_text
	histogram	Error	The server is in read-only mode.
SET LOCAL TRANSACTION READ WRITE;
INSERT INTO t1 (col1) VALUES (1);
ANALYZE TABLE t1 UPDATE HISTOGRAM ON col1 WITH 16 BUCKETS;
Table	Op	Msg_type	Msg_text
test.t1	histogram	status	Histogram statistics created for column 'col1'.
DROP TABLE t1;
#
# Additional tests for bug#29634540 "DROP DATABASE OF 1 MILLION
# TABLES ...".
#
# Check that DROP DATABASE statement doesn't acquire metadata locks
# on column statistics while dropping it.
#
CREATE DATABASE mysqltest;
CREATE TABLE mysqltest.t1 (i INT);
INSERT INTO mysqltest.t1 VALUES (1), (2), (3);
ANALYZE TABLE mysqltest.t1 UPDATE HISTOGRAM ON i WITH 10 BUCKETS;
Table	Op	Msg_type	Msg_text
mysqltest.t1	histogram	status	Histogram statistics created for column 'i'.
CREATE TABLE mysqltest.t2 (j INT);
# Run DROP DATABASE which will be paused near its end.
SET DEBUG_SYNC = 'rm_table_no_locks_before_binlog SIGNAL drop_waiting WAIT_FOR drop_resume';
DROP DATABASE mysqltest;
connect con1, localhost, root,,;
# Wait until DROP DATABASE gets paused and check what MDL
# it has acquired. There should be X locks on both tables
# and no locks on column statistics.
SET DEBUG_SYNC = 'now WAIT_FOR drop_waiting';
SELECT object_type, object_schema, object_name, column_name, lock_type
FROM performance_schema.metadata_locks
WHERE object_schema = "mysqltest"
  ORDER BY object_type, object_schema, object_name, column_name, lock_type;
object_type	object_schema	object_name	column_name	lock_type
SCHEMA	mysqltest	NULL	NULL	EXCLUSIVE
TABLE	mysqltest	t1	NULL	EXCLUSIVE
TABLE	mysqltest	t2	NULL	EXCLUSIVE
#
# Run concurrent ANALYZE TABLE ... DROP HISTOGRAM on table from
# database being dropped.
ANALYZE TABLE mysqltest.t1 DROP HISTOGRAM ON i;
connect con2, localhost, root,,;
# Check that it will be blocked thanks to table MDL.
# Unpause DROP DATABASE.
SET DEBUG_SYNC = 'now SIGNAL drop_resume';
disconnect con2;
connection con1;
# Reap ANALYZE TABLE ... DROP HISTOGRAM, which should not have
# found any histograms since database has been dropped.
Table	Op	Msg_type	Msg_text
mysqltest.t1	histogram	Error	No histogram statistics found for column 'i'.
disconnect con1;
connection default;
# Reap DROP DATABASE
# Clean-up.
SET DEBUG_SYNC = 'RESET';
