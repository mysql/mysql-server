#
# Some basic tests for the TempTable storage engine.
#
# Use "-- sorted_result" instead of "ORDER BY" because the latter may cause
# different execution plans.
#

-- let $is_debug = `SELECT VERSION() LIKE '%debug%'`

CREATE DATABASE temptable_test;
USE temptable_test;

-- exec $MYSQL temptable_test < $MYSQL_TEST_DIR/t/temptable_dump.sql

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

#

-- echo # Test 01
-- sorted_result
SELECT * FROM information_schema.table_constraints
WHERE table_schema = 'mysql' AND table_name != 'ndb_binlog_index'
ORDER BY table_schema,table_name,constraint_name COLLATE utf8mb3_general_ci;

-- echo # Test 03
SET optimizer_switch = 'derived_merge=off';
-- sorted_result
SELECT DISTINCT
  alias1.`col_int` AS field1,
  alias1.`pk` AS field2,
  alias1.`col_int` AS field3,
  alias1.`col_int_key` AS field4,
  alias1.`col_int_key` AS field5
FROM
  view_K AS alias1
  LEFT JOIN
  view_HH AS alias2
  ON
  alias1.`col_varchar_255_latin1` = alias2.`col_varchar_255_utf8_key`
WHERE alias1.`col_int` IS NULL
ORDER BY field1;
SET optimizer_switch = default;

-- echo # Test 04
-- sorted_result
SELECT
  GRANDPARENT1.`pk` AS g1,
  GRANDPARENT1.`col_datetime_key`
FROM
  CC AS GRANDPARENT1
  LEFT JOIN
  CC AS GRANDPARENT2
  USING (`col_int_key`)
WHERE
  GRANDPARENT1.`col_int_key` IN (
    SELECT PARENT1.`col_int_key` AS p1 FROM CC AS PARENT1
  ) AND GRANDPARENT1.`pk` <> 2
HAVING g1 <> 'p'
ORDER BY GRANDPARENT1.`col_datetime_key`;

-- echo # Test 05
-- sorted_result
SELECT
  GRANDPARENT1.`col_int_key` AS g1,
  GRANDPARENT1.`col_datetime_key` AS dt
FROM
  C AS GRANDPARENT1
  LEFT JOIN
  C AS GRANDPARENT2
  ON (GRANDPARENT2.`pk` <> GRANDPARENT1.`pk`)
WHERE
  (GRANDPARENT1.`pk`, GRANDPARENT1.`pk`) IN (
    SELECT DISTINCT
      PARENT1.`col_int_key` AS p1,
      PARENT1.`col_int_key` AS p2
    FROM
      C AS PARENT1
      LEFT JOIN
      C AS PARENT2
      USING (`col_varchar_key`)
    WHERE
      ((PARENT1.`pk` > GRANDPARENT1.`col_int_key`)
        OR ((PARENT1.`col_time_key` <= GRANDPARENT1.`col_time_key`)
             AND (PARENT1.`col_datetime_key` > '2005-02-01')
           )
      )
    ORDER BY PARENT1.`col_int_key`
  )
  AND GRANDPARENT1.`col_varchar_key` <> 'r'
HAVING g1 <> '13:16:53.053569'
ORDER BY GRANDPARENT1.`col_datetime_key`;

-- echo # Test 06
-- sorted_result
(SELECT DISTINCT
    *
FROM
    `view_table10000_innodb_int_autoinc`
WHERE
    (`col_varchar_10_key` LIKE CONCAT('Michigan', '%')
        OR `col_varchar_64_key` LIKE CONCAT('why', '%'))
        AND (`col_varchar_64_key` IS NOT NULL
        OR NOT (`col_varchar_64_key` = 'can\'t'))
        OR (`col_smallint_key` IN (1 , 244, 1, 1)
        OR `col_bigint_key` IS NOT NULL)
        AND (`col_bigint_key` IN (1 , - 89)
        OR (`col_bigint_key` != 1))
        AND (`col_varchar_10_key` IS NOT NULL
        AND `col_varchar_10_key` NOT IN ('Maine' , 'x'))
        AND (NOT (`col_bigint_key` = 1)
        AND `col_smallint_key` BETWEEN 1 AND 1 + 125)) UNION DISTINCT (SELECT
DISTINCT
    *
FROM
    `view_table10000_innodb_int_autoinc`
WHERE
    (`col_varchar_10_key` LIKE CONCAT('Michigan', '%')
        OR `col_varchar_64_key` LIKE CONCAT('why', '%'))
        AND (`col_varchar_64_key` IS NOT NULL
        OR NOT (`col_varchar_64_key` = 'can\'t'))
        OR (`col_smallint_key` IN (1 , 244, 1, 1)
        OR `col_bigint_key` IS NOT NULL)
        AND (`col_bigint_key` IN (1 , - 89)
        OR (`col_bigint_key` != 1))
        AND (`col_varchar_10_key` IS NOT NULL
        AND `col_varchar_10_key` NOT IN ('Maine' , 'x'))
        AND (NOT (`col_bigint_key` = 1)
        AND `col_smallint_key` BETWEEN 1 AND 1 + 125));

-- echo # Test 07
SET optimizer_switch = 'derived_merge=off';
-- sorted_result
SELECT
  alias2.`col_int_key`, alias2.pk, alias2.`col_varchar_10_latin1_key`
FROM
  MM AS alias1
  LEFT OUTER JOIN
  view_PP AS alias2
  ON alias1.`col_varchar_10_latin1` = alias2.`col_varchar_10_latin1_key`
WHERE alias2.`col_int` NOT IN (1);
SET optimizer_switch = default;

-- echo # Test 08
SET optimizer_switch = 'derived_merge=on';
-- sorted_result
SELECT
  alias2.`col_int_key`
FROM
  MM AS alias1
  LEFT OUTER JOIN
  view_PP AS alias2
  ON alias1.`col_varchar_10_latin1` = alias2.`col_varchar_10_latin1_key`
WHERE alias2.`col_int` NOT IN (1);
SET optimizer_switch = default;

-- echo # Test 09
SET optimizer_switch = 'derived_merge=off';
-- sorted_result
SELECT table1.pk
FROM view_D AS table1
LEFT JOIN D AS table2 ON table1.col_int_key = table2.col_int_key
WHERE table1.col_int_key IS NULL;

-- echo # Test 10
SET optimizer_switch = 'derived_merge=on';
-- sorted_result
SELECT table1.pk
FROM view_D AS table1
LEFT JOIN D AS table2 ON table1.col_int_key = table2.col_int_key
WHERE table1.col_int_key IS NULL;

-- echo # Test 11
CREATE TABLE t1 (
  pk int(11) NOT NULL DEFAULT '0',
  col_int_key int(11) DEFAULT NULL,
  col_varchar_key varchar(1) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
INSERT INTO t1 VALUES
(22,0,NULL),
(17,9,NULL),
(29,8,'c'),
(23,4,'d'),
(11,7,'d'),
(26,NULL,'f'),
(13,7,'f'),
(24,8,'g'),
(28,NULL,'j'),
(16,1,'m'),
(20,2,'m'),
(18,2,'o'),
(27,0,'p'),
(21,4,'q'),
(12,1,'r'),
(15,NULL,'u'),
(19,9,'w'),
(25,NULL,'x'),
(10,8,'x'),
(14,9,'y');
ANALYZE TABLE t1;
-- sorted_result
SELECT *
FROM (
  SELECT DISTINCT SUBQUERY1_t1.*
  FROM (
    t1 AS SUBQUERY1_t1
    LEFT OUTER JOIN
    t1 AS SUBQUERY1_t2
    ON (SUBQUERY1_t2.`pk` = SUBQUERY1_t1.`col_int_key`)
  )
) AS table1
WHERE table1.`col_varchar_key` IS NULL;
DROP TABLE t1;

-- echo # Test 12
CREATE TABLE t1 (
  id INT NOT NULL AUTO_INCREMENT,
  c1 CHAR(60) NOT NULL,
  c2 CHAR(60),
  PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
INSERT INTO t1 (c1, c2) VALUES
('abcdefghij', 'ABCDEFGHIJ'),
('mnopqrstuv', 'MNOPQRSTUV');
ANALYZE TABLE t1;
-- sorted_result
SELECT DISTINCT c1, c2 FROM t1 WHERE id BETWEEN 1 And 2 ORDER BY 1;
DROP TABLE t1;

#

-- disable_query_log
-- disable_result_log
if ($is_debug) {
  SET GLOBAL debug = default;
}
-- enable_result_log
-- enable_query_log

CREATE TABLE t1 (c1 VARCHAR(10) COLLATE utf8mb4_bin) ENGINE = InnoDB;
INSERT INTO t1 VALUES (''), (' ');
SELECT DISTINCT(c1) FROM t1;
DROP TABLE t1;

SET SESSION internal_tmp_mem_storage_engine = default;

USE test;
DROP DATABASE temptable_test;
