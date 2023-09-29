--source include/have_debug.inc
--source include/have_debug_sync.inc

--echo # Testing of histogram statistics that uses DEBUG functionality.

--echo #
--echo # Simulate a failure due to dropping histograms during DROP TABLE
--echo #
CREATE TABLE t1 (col1 INT, col2 INT);
ANALYZE TABLE t1 UPDATE HISTOGRAM ON col1, col2 WITH 10 BUCKETS;
SELECT schema_name, table_name, column_name
FROM information_schema.COLUMN_STATISTICS;

SELECT schema_name, table_name, column_name
FROM information_schema.COLUMN_STATISTICS;
SELECT COUNT(*) FROM information_schema.TABLES
WHERE TABLE_SCHEMA = 'test' AND TABLE_NAME = 't1';

SET DEBUG='+d,fail_after_drop_histograms';
--error ER_UNABLE_TO_DROP_COLUMN_STATISTICS
DROP TABLE t1;

SELECT schema_name, table_name, column_name
FROM information_schema.COLUMN_STATISTICS;
SELECT COUNT(*) FROM information_schema.TABLES
WHERE TABLE_SCHEMA = 'test' AND TABLE_NAME = 't1';

SET DEBUG='-d,fail_after_drop_histograms';

--echo #
--echo # Simulate a failure due to dropping histograms during ALTER TABLE
--echo #

SELECT schema_name, table_name, column_name
FROM information_schema.COLUMN_STATISTICS;
SELECT COUNT(*) FROM information_schema.COLUMNS
WHERE TABLE_SCHEMA = 'test' AND TABLE_NAME = 't1' AND COLUMN_NAME = 'col2';

SET DEBUG='+d,fail_after_drop_histograms';
--error ER_UNABLE_TO_DROP_COLUMN_STATISTICS
ALTER TABLE t1 DROP COLUMN col2;

SELECT schema_name, table_name, column_name
FROM information_schema.COLUMN_STATISTICS;
SELECT COUNT(*) FROM information_schema.COLUMNS
WHERE TABLE_SCHEMA = 'test' AND TABLE_NAME = 't1' AND COLUMN_NAME = 'col2';

SET DEBUG='-d,fail_after_drop_histograms';

--echo #
--echo # Simulate a failure due to renaming histograms during ALTER TABLE RENAME
--echo #
SELECT schema_name, table_name, column_name
FROM information_schema.COLUMN_STATISTICS;
SELECT COUNT(*) FROM information_schema.TABLES
WHERE TABLE_SCHEMA = 'test' AND TABLE_NAME = 't1';

SET DEBUG='+d,fail_after_rename_histograms';
--error ER_UNABLE_TO_UPDATE_COLUMN_STATISTICS
ALTER TABLE t1 RENAME TO t2;
SELECT schema_name, table_name, column_name
FROM information_schema.COLUMN_STATISTICS;
SELECT COUNT(*) FROM information_schema.TABLES
WHERE TABLE_SCHEMA = 'test' AND TABLE_NAME = 't1';
SET DEBUG='-d,fail_after_rename_histograms';

DROP TABLE t1;


--echo #
--echo # Check that histogram with sampling works as expected
--echo #

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
SELECT schema_name, table_name, column_name,
       JSON_REMOVE(histogram, '$."last-updated"')
FROM information_schema.COLUMN_STATISTICS;

SET DEBUG='-d,histogram_force_sampling';
DROP TABLE t1;

--echo #
--echo # Bug#26020352 WL8943:ASSERTION `M_THD->GET_TRANSACTION()->IS_EMPTY(
--echo #              TRANSACTION_CTX::STMT) && M
--echo #
CREATE TABLE t1 (col1 INT);
ANALYZE TABLE t1 UPDATE HISTOGRAM ON col1 WITH 10 BUCKETS;
SELECT schema_name, table_name, column_name
FROM information_schema.COLUMN_STATISTICS;
SET DEBUG='+d,histogram_fail_after_open_table';
ANALYZE TABLE t1 UPDATE HISTOGRAM ON col1 WITH 10 BUCKETS;
SELECT schema_name, table_name, column_name
FROM information_schema.COLUMN_STATISTICS;
SET DEBUG='-d,histogram_fail_after_open_table';
DROP TABLE t1;

--echo #
--echo # Bug#26027240 WL8943:VIRTUAL BOOL SQL_CMD_ANALYZE_TABLE::EXECUTE(THD*):
--echo #              ASSERTION `FALSE' FAIL
--echo #
CREATE TABLE t1 (col1 INT);
ANALYZE TABLE t1 UPDATE HISTOGRAM ON col1 WITH 10 BUCKETS;
SET DEBUG='+d,histogram_fail_during_lock_for_write';
ANALYZE TABLE t1 DROP HISTOGRAM ON col1;

--echo # Since we have simulated a fail, the histogram should still be present.
--echo # However, since this is a simulation of failure no error is reported.
SELECT schema_name, table_name, column_name
FROM information_schema.COLUMN_STATISTICS;
SET DEBUG='-d,histogram_fail_during_lock_for_write';
ANALYZE TABLE t1 DROP HISTOGRAM ON col1;

--echo # The histogram should now be gone.
SELECT schema_name, table_name, column_name
FROM information_schema.COLUMN_STATISTICS;
DROP TABLE t1;


--echo #
--echo # Bug#26772858 MDL FOR COLUMN STATISTICS IS NOT PROPERLY REFLECTED IN
--echo # P_S.METADATA_LOCKS
--echo #
connect(con1, localhost, root,,);
CREATE TABLE t1 (col1 INT);
SET DEBUG_SYNC='store_histogram_after_write_lock SIGNAL histogram_1_waiting WAIT_FOR continue_store_histogram';
--send ANALYZE TABLE t1 UPDATE HISTOGRAM ON col1 WITH 2 BUCKETS;

# The connection 'con1' will now wait on the debug sync point
# "store_histogram_after_write_lock", where it has acquired an exclusive lock
# on the histogram object. Switch connection, and inspect the metadata locks
# table in performance schema in order to verify that OBJECT_TYPE is properly
# reflected. Wait until 'con1' has signaled that it actually is waiting
--connection default
SET DEBUG_SYNC='now WAIT_FOR histogram_1_waiting';
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, COLUMN_NAME
  FROM performance_schema.metadata_locks
  WHERE LOCK_TYPE = "EXCLUSIVE"
    AND OBJECT_TYPE = "COLUMN STATISTICS"
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, COLUMN_NAME;

# While 'con1' still is waiting on the sync point right after the exclusive lock
# is aquired, open a new connection and create a histogram for the same column.
# The effect we want is a wait on the same MDL, so that we can inspect that
# the lock is fully reflected in performance_schema.events_waits_*
connect(con2, localhost, root,,);
SET DEBUG_SYNC='mdl_acquire_lock_wait SIGNAL histogram_2_lock_waiting';
--send ANALYZE TABLE t1 UPDATE HISTOGRAM ON col1 WITH 2 BUCKETS;

# Go back to the default connection, and verify the contents of
# performance_schema.events_waits_*. Wait until 'con2' has signaled that it is
# actually waiting for the lock.
--connection default
SET DEBUG_SYNC='now WAIT_FOR histogram_2_lock_waiting';
SELECT OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, INDEX_NAME
  FROM performance_schema.events_waits_current
  WHERE OBJECT_TYPE = "COLUMN STATISTICS"
  ORDER BY OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, INDEX_NAME;

# Finally let 'con1' finish building the histogram. Once 'con1' has released the
# MDL on the column statistics object, 'con2' will continue and do its work.
SET DEBUG_SYNC='now SIGNAL continue_store_histogram';

--connection con1
--reap
--disconnect con1
--source include/wait_until_disconnected.inc

--connection con2
--reap
--disconnect con2
--source include/wait_until_disconnected.inc

--connection default
DROP TABLE t1;

--echo #
--echo # Bug#27672693  HISTOGRAMS: ASSERTION FAILED: !THD->TX_READ_ONLY
--echo #
CREATE TABLE t1(col1 INT);
SET LOCAL TRANSACTION READ ONLY;
--error ER_CANT_EXECUTE_IN_READ_ONLY_TRANSACTION
INSERT INTO t1 (col1) VALUES (1);
ANALYZE TABLE t1 UPDATE HISTOGRAM ON col1 WITH 16 BUCKETS;
SET LOCAL TRANSACTION READ WRITE;
INSERT INTO t1 (col1) VALUES (1);
ANALYZE TABLE t1 UPDATE HISTOGRAM ON col1 WITH 16 BUCKETS;
DROP TABLE t1;

--echo #
--echo # Additional tests for bug#29634540 "DROP DATABASE OF 1 MILLION
--echo # TABLES ...".
--echo #
--echo # Check that DROP DATABASE statement doesn't acquire metadata locks
--echo # on column statistics while dropping it.
--echo #
--enable_connect_log
CREATE DATABASE mysqltest;
CREATE TABLE mysqltest.t1 (i INT);
INSERT INTO mysqltest.t1 VALUES (1), (2), (3);
ANALYZE TABLE mysqltest.t1 UPDATE HISTOGRAM ON i WITH 10 BUCKETS;
CREATE TABLE mysqltest.t2 (j INT);
--echo # Run DROP DATABASE which will be paused near its end.
SET DEBUG_SYNC = 'rm_table_no_locks_before_binlog SIGNAL drop_waiting WAIT_FOR drop_resume';
--send DROP DATABASE mysqltest

--connect(con1, localhost, root,,)
--echo # Wait until DROP DATABASE gets paused and check what MDL
--echo # it has acquired. There should be X locks on both tables
--echo # and no locks on column statistics.
SET DEBUG_SYNC = 'now WAIT_FOR drop_waiting';
SELECT object_type, object_schema, object_name, column_name, lock_type
  FROM performance_schema.metadata_locks
  WHERE object_schema = "mysqltest"
  ORDER BY object_type, object_schema, object_name, column_name, lock_type;

--echo #
--echo # Run concurrent ANALYZE TABLE ... DROP HISTOGRAM on table from
--echo # database being dropped.
--send ANALYZE TABLE mysqltest.t1 DROP HISTOGRAM ON i

--connect(con2, localhost, root,,)
--echo # Check that it will be blocked thanks to table MDL.
let $wait_condition=
  SELECT COUNT(*) = 1 FROM information_schema.processlist
  WHERE state = "Waiting for table metadata lock" and
        info = "ANALYZE TABLE mysqltest.t1 DROP HISTOGRAM ON i";
--source include/wait_condition.inc

--echo # Unpause DROP DATABASE.
SET DEBUG_SYNC = 'now SIGNAL drop_resume';

--disconnect con2
--source include/wait_until_disconnected.inc

--connection con1
--echo # Reap ANALYZE TABLE ... DROP HISTOGRAM, which should not have
--echo # found any histograms since database has been dropped.
--reap

--disconnect con1
--source include/wait_until_disconnected.inc

--connection default
--echo # Reap DROP DATABASE
--reap

--echo # Clean-up.
SET DEBUG_SYNC = 'RESET';
--disable_connect_log

-- echo #
-- echo # Bug#35419418 CH Benchmark failing with crash in histogram code of mysql optimizer
-- echo #
-- echo # When updating an existing string histogram we access freed memory
-- echo # which can lead to a crash. This test updates equi-height and
-- echo # singleton histograms twice on all supported column types.
-- echo # Without the bugfix this test will sometimes cause a crash and will
-- echo # produce a "heap-use-after-free" error in ASAN builds.

CREATE TABLE all_types (
  col_bool BOOLEAN,
  col_bit BIT(4),
  col_tinyint TINYINT,
  col_smallint SMALLINT,
  col_mediumint MEDIUMINT,
  col_integer INTEGER,
  col_bigint BIGINT,
  col_tinyint_unsigned TINYINT UNSIGNED,
  col_smallint_unsigned SMALLINT UNSIGNED,
  col_mediumint_unsigned MEDIUMINT UNSIGNED,
  col_integer_unsigned INTEGER UNSIGNED,
  col_bigint_unsigned BIGINT UNSIGNED,
  col_float FLOAT,
  col_double DOUBLE,
  col_decimal DECIMAL(2, 2),
  col_date DATE,
  col_time TIME,
  col_year YEAR,
  col_datetime DATETIME,
  col_timestamp TIMESTAMP NULL,
  col_char CHAR(255),
  col_varchar VARCHAR(255),
  col_tinytext TINYTEXT,
  col_text TEXT,
  col_mediumtext MEDIUMTEXT,
  col_longtext LONGTEXT,
  col_binary BINARY(255),
  col_varbinary VARBINARY(255),
  col_tinyblob TINYBLOB,
  col_blob BLOB,
  col_mediumblob MEDIUMBLOB,
  col_longblob LONGBLOB,
  col_enum ENUM('zero', 'one', 'two'),
  col_set SET('zero', 'one', 'two')
);

-- echo # Insert 3 different values into each column (except for BOOLEAN) so
-- echo # that we get equi-height histograms when we call UPDATE HISTOGRAM ON
-- echo # ... WITH 2 BUCKETS and singleton histograms when we use WITH 4 BUCKETS.

INSERT INTO all_types VALUES (
  FALSE,                 # BOOLEAN
  b'0000',               # BIT
  0,                     # TINYINT
  0,                     # SMALLINT
  0,                     # MEDIUMINT
  0,                     # INTEGER
  0,                     # BIGINT
  0,                     # TINYINT_UNSIGNED
  0,                     # SMALLINT_UNSIGNED
  0,                     # MEDIUMINT_UNSIGNED
  0,                     # INTEGER_UNSIGNED
  0,                     # BIGINT_UNSIGNED
  0,                     # FLOAT
  0,                     # DOUBLE
  00.00,                 # DECIMAL(2, 2)
  '1000-01-01',          # DATE
  '00:00:00.000000',     # TIME
  1901,                  # YEAR
  '1000-01-01 00:00:00', # DATETIME
  '1971-01-01 00:00:00', # TIMESTAMP
  '0',                   # CHAR
  '0',                   # VARCHAR
  '0',                   # TINYTEXT
  '0',                   # TEXT
  '0',                   # MEDIUMTEXT
  '0',                   # LONGTEXT
  '0',                   # BINARY
  '0',                   # VARBINARY
  '0',                   # TINYBLOB
  '0',                   # BLOB
  '0',                   # MEDIUMBLOB
  '0',                   # LONGBLOB
  'zero',                 # ENUM
  'zero'                  # SET
);

INSERT INTO all_types VALUES (
  TRUE,                  # BOOLEAN
  b'0001',               # BIT
  1,                     # TINYINT
  1,                     # SMALLINT
  1,                     # MEDIUMINT
  1,                     # INTEGER
  1,                     # BIGINT
  1,                     # TINYINT_UNSIGNED
  1,                     # SMALLINT_UNSIGNED
  1,                     # MEDIUMINT_UNSIGNED
  1,                     # INTEGER_UNSIGNED
  1,                     # BIGINT_UNSIGNED
  1,                     # FLOAT
  1,                     # DOUBLE
  00.01,                 # DECIMAL(2, 2)
  '1001-01-01',          # DATE
  '00:00:00.000001',     # TIME
  1902,                  # YEAR
  '1001-01-01 00:00:00', # DATETIME
  '1971-01-01 00:00:01', # TIMESTAMP
  '1',                   # CHAR
  '1',                   # VARCHAR
  '1',                   # TINYTEXT
  '1',                   # TEXT
  '1',                   # MEDIUMTEXT
  '1',                   # LONGTEXT
  '1',                   # BINARY
  '1',                   # VARBINARY
  '1',                   # TINYBLOB
  '1',                   # BLOB
  '1',                   # MEDIUMBLOB
  '1',                   # LONGBLOB
  'one',                 # ENUM
  'one'                  # SET
);

INSERT INTO all_types VALUES (
  TRUE,                  # BOOLEAN
  b'0010',               # BIT
  2,                     # TINYINT
  2,                     # SMALLINT
  2,                     # MEDIUMINT
  2,                     # INTEGER
  2,                     # BIGINT
  2,                     # TINYINT_UNSIGNED
  2,                     # SMALLINT_UNSIGNED
  2,                     # MEDIUMINT_UNSIGNED
  2,                     # INTEGER_UNSIGNED
  2,                     # BIGINT_UNSIGNED
  2,                     # FLOAT
  2,                     # DOUBLE
  00.02,                 # DECIMAL(2, 2)
  '1002-01-01',          # DATE
  '00:00:00.000002',     # TIME
  1903,                  # YEAR
  '1002-01-01 00:00:00', # DATETIME
  '1971-01-01 00:00:02', # TIMESTAMP
  '2',                   # CHAR
  '2',                   # VARCHAR
  '2',                   # TINYTEXT
  '2',                   # TEXT
  '2',                   # MEDIUMTEXT
  '2',                   # LONGTEXT
  '2',                   # BINARY
  '2',                   # VARBINARY
  '2',                   # TINYBLOB
  '2',                   # BLOB
  '2',                   # MEDIUMBLOB
  '2',                   # LONGBLOB
  'two',                 # ENUM
  'two'                  # SET
);

--echo #
--echo # Build singleton histograms.
--echo #
ANALYZE TABLE all_types UPDATE HISTOGRAM ON
col_bool,
col_bit,
col_tinyint,
col_smallint,
col_mediumint,
col_integer,
col_bigint,
col_tinyint_unsigned,
col_smallint_unsigned,
col_mediumint_unsigned,
col_integer_unsigned,
col_bigint_unsigned,
col_float,
col_double,
col_decimal,
col_date,
col_time,
col_year,
col_datetime,
col_timestamp,
col_char,
col_varchar,
col_tinytext,
col_text,
col_mediumtext,
col_longtext,
col_binary,
col_varbinary,
col_tinyblob,
col_blob,
col_mediumblob,
col_longblob,
col_enum,
col_set
WITH 4 BUCKETS;

ANALYZE TABLE all_types UPDATE HISTOGRAM ON
col_bool,
col_bit,
col_tinyint,
col_smallint,
col_mediumint,
col_integer,
col_bigint,
col_tinyint_unsigned,
col_smallint_unsigned,
col_mediumint_unsigned,
col_integer_unsigned,
col_bigint_unsigned,
col_float,
col_double,
col_decimal,
col_date,
col_time,
col_year,
col_datetime,
col_timestamp,
col_char,
col_varchar,
col_tinytext,
col_text,
col_mediumtext,
col_longtext,
col_binary,
col_varbinary,
col_tinyblob,
col_blob,
col_mediumblob,
col_longblob,
col_enum,
col_set
WITH 4 BUCKETS;

--sorted_result
SELECT schema_name, table_name, column_name,
JSON_EXTRACT(histogram, '$."histogram-type"') AS should_be_singleton
FROM information_schema.column_statistics;

ANALYZE TABLE all_types DROP HISTOGRAM ON
col_bool,
col_bit,
col_tinyint,
col_smallint,
col_mediumint,
col_integer,
col_bigint,
col_tinyint_unsigned,
col_smallint_unsigned,
col_mediumint_unsigned,
col_integer_unsigned,
col_bigint_unsigned,
col_float,
col_double,
col_decimal,
col_date,
col_time,
col_year,
col_datetime,
col_timestamp,
col_char,
col_varchar,
col_tinytext,
col_text,
col_mediumtext,
col_longtext,
col_binary,
col_varbinary,
col_tinyblob,
col_blob,
col_mediumblob,
col_longblob,
col_enum,
col_set;

--echo #
--echo # Build equi-height histograms.
--echo #
ANALYZE TABLE all_types UPDATE HISTOGRAM ON
col_bool,
col_bit,
col_tinyint,
col_smallint,
col_mediumint,
col_integer,
col_bigint,
col_tinyint_unsigned,
col_smallint_unsigned,
col_mediumint_unsigned,
col_integer_unsigned,
col_bigint_unsigned,
col_float,
col_double,
col_decimal,
col_date,
col_time,
col_year,
col_datetime,
col_timestamp,
col_char,
col_varchar,
col_tinytext,
col_text,
col_mediumtext,
col_longtext,
col_binary,
col_varbinary,
col_tinyblob,
col_blob,
col_mediumblob,
col_longblob,
col_enum,
col_set
WITH 2 BUCKETS;

ANALYZE TABLE all_types UPDATE HISTOGRAM ON
col_bool,
col_bit,
col_tinyint,
col_smallint,
col_mediumint,
col_integer,
col_bigint,
col_tinyint_unsigned,
col_smallint_unsigned,
col_mediumint_unsigned,
col_integer_unsigned,
col_bigint_unsigned,
col_float,
col_double,
col_decimal,
col_date,
col_time,
col_year,
col_datetime,
col_timestamp,
col_char,
col_varchar,
col_tinytext,
col_text,
col_mediumtext,
col_longtext,
col_binary,
col_varbinary,
col_tinyblob,
col_blob,
col_mediumblob,
col_longblob,
col_enum,
col_set
WITH 2 BUCKETS;

--sorted_result
SELECT schema_name, table_name, column_name,
JSON_EXTRACT(histogram, '$."histogram-type"') AS should_be_equiheight
FROM information_schema.column_statistics;

DROP TABLE all_types;

--echo #
--echo # Verify that we can update existing histograms twice with USING DATA.
--echo #

CREATE TABLE t(x VARCHAR(8));
INSERT INTO t VALUES ('a'), ('b'), ('c');

--echo # USING DATA with singleton string histogram.
ANALYZE TABLE t UPDATE HISTOGRAM ON x USING DATA '{"buckets": [["base64:type254:MA==", 0.3333333333333333], ["base64:type254:MQ==", 0.6666666666666666], ["base64:type254:Mg==", 1.0]], "data-type": "string", "auto-update": false, "null-values": 0.0, "collation-id": 255, "sampling-rate": 1.0, "histogram-type": "singleton", "number-of-buckets-specified": 4}';
ANALYZE TABLE t UPDATE HISTOGRAM ON x USING DATA '{"buckets": [["base64:type254:MA==", 0.3333333333333333], ["base64:type254:MQ==", 0.6666666666666666], ["base64:type254:Mg==", 1.0]], "data-type": "string", "auto-update": false, "null-values": 0.0, "collation-id": 255, "sampling-rate": 1.0, "histogram-type": "singleton", "number-of-buckets-specified": 4}';
ANALYZE TABLE t DROP HISTOGRAM ON x;

--echo # USING DATA with equi-height string histogram.
ANALYZE TABLE t UPDATE HISTOGRAM ON x USING DATA '{"buckets": [["base64:type254:MA==", "base64:type254:MQ==", 0.6666666666666666, 2], ["base64:type254:Mg==", "base64:type254:Mg==", 1.0, 1]], "data-type": "string", "auto-update": false, "null-values": 0.0, "collation-id": 255, "sampling-rate": 1.0, "histogram-type": "equi-height", "number-of-buckets-specified": 2}';
ANALYZE TABLE t UPDATE HISTOGRAM ON x USING DATA '{"buckets": [["base64:type254:MA==", "base64:type254:MQ==", 0.6666666666666666, 2], ["base64:type254:Mg==", "base64:type254:Mg==", 1.0, 1]], "data-type": "string", "auto-update": false, "null-values": 0.0, "collation-id": 255, "sampling-rate": 1.0, "histogram-type": "equi-height", "number-of-buckets-specified": 2}';
DROP TABLE t;

CREATE TABLE t(x INT);
INSERT INTO t VALUES (1), (2), (3);

--echo # USING DATA with singleton integer histogram.
ANALYZE TABLE t UPDATE HISTOGRAM ON x USING DATA '{"buckets": [[0, 0.3333333333333333], [1, 0.6666666666666666], [2, 1.0]], "data-type": "int", "auto-update": false, "null-values": 0.0, "collation-id": 8, "sampling-rate": 1.0, "histogram-type": "singleton", "number-of-buckets-specified": 4}';
ANALYZE TABLE t UPDATE HISTOGRAM ON x USING DATA '{"buckets": [[0, 0.3333333333333333], [1, 0.6666666666666666], [2, 1.0]], "data-type": "int", "auto-update": false, "null-values": 0.0, "collation-id": 8, "sampling-rate": 1.0, "histogram-type": "singleton", "number-of-buckets-specified": 4}';
ANALYZE TABLE t DROP HISTOGRAM ON x;

--echo # USING DATA with equi-height integer histogram.
ANALYZE TABLE t UPDATE HISTOGRAM ON x USING DATA '{"buckets": [[0, 1, 0.6666666666666666, 2], [2, 2, 1.0, 1]], "data-type": "int", "auto-update": false, "null-values": 0.0, "collation-id": 8, "sampling-rate": 1.0, "histogram-type": "equi-height", "number-of-buckets-specified": 2}';
ANALYZE TABLE t UPDATE HISTOGRAM ON x USING DATA '{"buckets": [[0, 1, 0.6666666666666666, 2], [2, 2, 1.0, 1]], "data-type": "int", "auto-update": false, "null-values": 0.0, "collation-id": 8, "sampling-rate": 1.0, "histogram-type": "equi-height", "number-of-buckets-specified": 2}';
DROP TABLE t;


-- echo #
-- echo # Bug#35227319 String histogram collation mismatch on comparison with REVERSE(1)
-- echo #
CREATE TABLE t1(x TEXT);
INSERT INTO t1 VALUES ("1");
ANALYZE TABLE t1 UPDATE HISTOGRAM ON x;
SELECT * FROM t1 AS a JOIN t1 AS b ON a.x = REVERSE(1);
DROP TABLE t1;
