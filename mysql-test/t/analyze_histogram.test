# We need the Debug Sync Facility.
--source include/have_debug_sync.inc

--echo #
--echo # Bug#34288890 Histogram commands invalidate TABLE_SHARE
--echo #

--echo #
--echo # Verify that new queries do not wait for old queries to terminate.
--echo #

CREATE TABLE t1 (c1 INT) ENGINE=INNODB;
INSERT INTO t1 VALUES (1), (2);
ANALYZE TABLE t1;

--echo # Case 1/2: ANALYZE TABLE UPDATE HISTOGRAM

--enable_connect_log
--connect (con1,localhost,root)
SET DEBUG_SYNC="before_reset_query_plan SIGNAL first_select_ongoing WAIT_FOR second_select_finished";
--send SELECT c1 FROM t1

--connection default
SET DEBUG_SYNC="now WAIT_FOR first_select_ongoing";
ANALYZE TABLE t1 UPDATE HISTOGRAM ON c1;

# Without the patch, this SELECT would wait indefinitely.
SELECT c1 FROM t1;

SET DEBUG_SYNC="now SIGNAL second_select_finished";

--connection con1
--reap;

--echo # Case 2/2: ANALYZE TABLE DROP HISTOGRAM

--connection con1
SET DEBUG_SYNC="before_reset_query_plan SIGNAL first_select_ongoing WAIT_FOR second_select_finished";
--send SELECT c1 FROM t1

--connection default
SET DEBUG_SYNC="now WAIT_FOR first_select_ongoing";
ANALYZE TABLE t1 DROP HISTOGRAM ON c1;

# Without the patch, this SELECT would wait indefinitely.
SELECT c1 FROM t1;

SET DEBUG_SYNC="now SIGNAL second_select_finished";

--connection con1
--reap;

--echo #
--echo # Different TABLE objects can use different histograms.
--echo #

--connection con1
ANALYZE TABLE t1 UPDATE HISTOGRAM ON c1;
SET DEBUG_SYNC="after_table_open SIGNAL first_histogram_acquired WAIT_FOR second_histogram_acquired";
--send EXPLAIN SELECT c1 FROM t1 WHERE c1 < 3;

--connection default
SET DEBUG_SYNC="now WAIT_FOR first_histogram_acquired";
UPDATE t1 SET c1 = 3 WHERE c1 = 2;
ANALYZE TABLE t1 UPDATE HISTOGRAM ON c1;
--echo # Selectivity estimate (filtered) should be 50.00
SET DEBUG_SYNC="after_table_open SIGNAL second_histogram_acquired";
EXPLAIN SELECT c1 FROM t1 WHERE c1 < 3;

--connection con1
--echo # Selectivity estimate (filtered) should be 100.00
--reap;

--disconnect con1
--source include/wait_until_disconnected.inc
--connection default
--disable_connect_log

DROP TABLE t1;
