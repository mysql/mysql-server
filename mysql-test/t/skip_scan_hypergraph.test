--source include/have_hypergraph.inc

let $engine=innodb;

--echo # Note: A lot of these tests changed with the new cost model.
--echo # Following the cost model patches we can no longer trust
--echo # that the chosen plans matches the intended plan for these tests.
--echo #
--echo # TODO(tlchrist): Find a way to make these tests stable
--echo # and maintainable under changes to the cost model and statistics.

--source include/skip_scan_test.inc

--echo Skip scan with subqueries

CREATE TABLE `t1` (
  `pk` int NOT NULL AUTO_INCREMENT,
  `col_int_key` int DEFAULT NULL,
  `col_varchar_key` varchar(1) DEFAULT NULL,
  PRIMARY KEY (`pk`),
  KEY `idx_t1_col_int_key` (`col_int_key`),
  KEY `idx_t1_col_varchar_key` (`col_varchar_key`));

CREATE TABLE `t2` (
  `pk` int NOT NULL AUTO_INCREMENT,
  `col_varchar_key` varchar(1) DEFAULT NULL,
  PRIMARY KEY (`pk`),
  KEY `idx_t2_col_varchar_key` (`col_varchar_key`));

# Bug#35004730 Assertion failed: (read_rows >= 0.0) in opt_costmodel.h
SELECT SUBQUERY1_t1.col_varchar_key AS SUBQUERY1_field1
FROM t2 AS SUBQUERY1_t1
WHERE (SUBQUERY1_t1.pk > 5 AND
       SUBQUERY1_t1.col_varchar_key NOT IN (SELECT CHILD_SUBQUERY1_t1 .col_varchar_key AS CHILD_SUBQUERY1_field1
                                            FROM t1 AS CHILD_SUBQUERY1_t1
                                            WHERE CHILD_SUBQUERY1_t1.col_int_key = 3));

# Bug#35005471 Assertion failed: (x >= 0) function:FuzzyComparison, file:compare_access_path.h
SELECT SUM(table1.pk )AS field1 , COUNT(table2.col_varchar_key ) AS field2
FROM (t1 AS table1 JOIN (t2 AS table2))
WHERE (table2.col_varchar_key <> SOME (SELECT SUBQUERY2_t1. col_varchar_key AS SUBQUERY2_field1
                   FROM t1 AS SUBQUERY2_t1
                   WHERE (SUBQUERY2_t1.pk < 4
                          AND SUBQUERY2_t1. pk IN (SELECT 2 EXCEPT SELECT 9))));

DROP TABLE t1;
DROP TABLE t2;

SET OPTIMIZER_SWITCH = default;

--source include/disable_hypergraph.inc
