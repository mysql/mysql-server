--source include/force_myisam_default.inc
--source include/have_myisam.inc

#
# Some basic tests for the TempTable storage engine.
#
# Use "-- sorted_result" instead of "ORDER BY" because the latter may cause
# different execution plans.
#

-- let $is_debug = `SELECT VERSION() LIKE '%debug%'`

CREATE DATABASE temptable_test;
USE temptable_test;

-- exec $MYSQL temptable_test <  $MYSQL_TEST_DIR/t/temptable_dump.sql

-- disable_query_log
-- disable_result_log
ANALYZE TABLE A;
ANALYZE TABLE AA;
ANALYZE TABLE B;
ANALYZE TABLE BB;
ANALYZE TABLE C;
ANALYZE TABLE CC;
ANALYZE TABLE D;
ANALYZE TABLE DD;
ANALYZE TABLE DUMMY;
ANALYZE TABLE E;
ANALYZE TABLE HH;
ANALYZE TABLE K;
ANALYZE TABLE MM;
ANALYZE TABLE PP;
ANALYZE TABLE table10000_innodb_int_autoinc;

SET SESSION internal_tmp_mem_storage_engine = 'TempTable';

# Several of the DBUG_PRINT calls give 'conditional jump'
if (!$VALGRIND_TEST) {
  if ($is_debug) {
    SET GLOBAL debug = "+d,temptable_api";
  }
}

-- enable_result_log
-- enable_query_log



-- echo # Test 02
SET optimizer_switch = "derived_merge=off";
CREATE TABLE t1 (
  col1 BIGINT NOT NULL,
  col2 BIGINT NOT NULL
) ENGINE=MyISAM;
INSERT INTO t1 VALUES (8, 109), (4, 98), (4, 120), (7, 103), (4, 112);
CREATE VIEW v1 AS SELECT * FROM t1;
-- sorted_result
SELECT col1
FROM t1
WHERE ( NOT EXISTS (
  SELECT col2
  FROM t1
  WHERE ( 7 ) IN (
    SELECT v1.col1
    FROM ( v1 JOIN ( SELECT * FROM t1 ) AS d1
      ON ( d1.col2 = v1.col2 )
    )
  )
));
DROP VIEW v1;
DROP TABLE t1;
SET optimizer_switch = default;

-- disable_query_log
-- disable_result_log
if ($is_debug) {
  SET GLOBAL debug = default;
}
-- enable_result_log
-- enable_query_log

SET SESSION internal_tmp_mem_storage_engine = default;


USE test;
DROP DATABASE temptable_test;
