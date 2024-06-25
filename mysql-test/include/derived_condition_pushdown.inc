# Tests for WL#8084 - Condition pushdown to derived table
--source include/elide_costs.inc

#setup tables
CREATE TABLE t0 (
  i0 INTEGER
);

INSERT INTO t0 VALUES (0),(1),(2),(3),(4);

CREATE TABLE t1 (f1 INTEGER, f2 INTEGER, f3 INTEGER,
                 KEY(f1), KEY(f1,f2), KEY(f3));
INSERT INTO t1
SELECT i0, i0 + 10*i0,
       i0 + 10*i0 + 100*i0
FROM t0 AS a0;
INSERT INTO t1
SELECT i0, i0 + 10*i0,
       i0 + 10*i0 + 100*i0
FROM t0 AS a0;
INSERT INTO t1 VALUES (NULL, 1, 2);
INSERT INTO t1 VALUES (NULL, 1, 3);

ANALYZE TABLE t0, t1;

SET optimizer_switch="derived_merge=off,derived_condition_pushdown=on";
# Test with simple WHERE condition that needs to be pushed
let query = SELECT * FROM (SELECT f1, f2 FROM t1) as dt WHERE f1 > 2;
# Masking all cost output as it makes the test unstable -
# most likely because they differ with innodb_page_size
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;

# Test with a condition that has AND. Entire condition is pushed to
# derived table.
let query = SELECT * FROM (SELECT f1, f2 FROM t1) as dt
            WHERE f1 < 3 and f2 > 11;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# Same as above with three conditions in an AND
let query = SELECT * FROM (SELECT f1, f2, f3 FROM t1) as dt
            WHERE f1 > 2 and f2 < 25 and f3 > 200;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# Same as above with three conditions with both AND and OR
let query = SELECT * FROM (SELECT f1, f2, f3 FROM t1) as dt
            WHERE f1 > 3 and f2 < 50 or f3 > 200;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;
let query = SELECT t1.f2 as f2, dt.f1 as f1, dt.f3 as f3 FROM t1,
            (SELECT f1, f2, f3 FROM t1) as dt
            WHERE (dt.f1 = 6) or( t1.f2 = 50 and dt.f3 = 200);
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# Test for pushing condition down partially
# Fix test instability with join order hint
let query = SELECT /*+ JOIN_ORDER(t0, dt) */ * FROM
            (SELECT f1, f2, f3 FROM t1) as dt, t0
            WHERE f1 > 3 and f2 < 50 and i0 > 3;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# Test with nested derived tables with simple WHERE condition
let query = SELECT * FROM (SELECT * FROM (SELECT * FROM t1) as dt1) as dt2
            WHERE f1 > 3 and f2 < 50 and f3 > 200;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# Simple derived tables with complex WHERE conditions
let query = SELECT * FROM (SELECT f1, f2, f3 FROM t1) as dt
            WHERE (f1 > 2 and f2 < 35) and (f1+f3) > 300;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

let query = SELECT * FROM (SELECT f1, f2, f3 FROM t1) as dt
            WHERE (f1 > 2 and f2 < 35) or (f1+f3) > 300 or (f1 < 2);
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

let query = SELECT * FROM (SELECT f1, f2 FROM t1) as dt1,
                          (SELECT f3 FROM t1) as dt2
                          WHERE (f1 > 2 and f2 < 35) and (f1+f3) > 300
                          and (f3 < 400);
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# Nested derived tables with complex WHERE conditions
let query = SELECT * FROM (SELECT * FROM (SELECT f1, f2 FROM t1) as dt1,
                                         (SELECT f3 FROM t1) as dt2) as dt3
                          WHERE (f1 > 2 and f2 < 35) and (f1+f3) > 200
                               and (f3 < 300);
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# Nested derived table with fields that have different aliases
let query = SELECT i, j, k FROM (SELECT f1 as i, f2 as j, f2 as k FROM t1) as dt
            WHERE i > 1 and i+j > 40;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

let query = SELECT i, j, k
            FROM (SELECT l as i, m as j, n as k
                  FROM (SELECT f1 as l, f2 as m, f3 as n
                        FROM t1) as dt1 ) as dt2
            WHERE i > 1 and i+j > 40;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

let query = SELECT i, j, l, m, n
           FROM (SELECT f1 as i, f2 as j FROM t1 ) as dt1 ,
                (SELECT f1 as l, f2 as m, f3 as n FROM t1) as dt2
           WHERE i > 1 and i+j > 40 and m < 20 and m+i > 20;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;
let query = SELECT * FROM
(SELECT (i+j) AS g, f1 FROM
 (SELECT (f1+f2) AS i, (f1-f2) AS j FROM
  (SELECT f1+10 AS f1, f2+10 AS f2 FROM t1) AS dt0)
 AS dt1,
 (SELECT f1, f2 FROM t1) AS dt2) AS dt3 WHERE g > 26 and g+f1 > 31;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;


# Test with nested derived tables that are expressions in underlying derived
# tables.
let query = SELECT l, m  FROM (SELECT (i+3) as l, (j+4) as m FROM
                               (SELECT (f1+f2) as i, (f3+10) as j FROM t1) as
                               dt1) as dt2 WHERE l > 20 and l+m > 10 ;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# Test with projection list for derived tables
let query = SELECT i FROM (SELECT f1 FROM t1) as dt(i) WHERE i > 10;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

let query = SELECT m FROM (SELECT k+2 as l
                           FROM (SELECT f1+f2 as j
                                 FROM t1) as dt1(k)) as dt(m)
            WHERE m > 30;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;

# Test with aggregated query
let query = SELECT f1 FROM (SELECT f1, SUM(f2) FROM t1 GROUP BY f1) as dt
            WHERE f1 > 3;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;

let query = SELECT f1 FROM (SELECT f1, f2, SUM(f3) FROM t1 GROUP BY f1,f2) as dt
            WHERE f1+f2 > 30;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;

let query = SELECT f1
           FROM (SELECT f1, SUM(f2) FROM t1 GROUP BY f1 WITH ROLLUP) as dt
           WHERE f1 IS NULL;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

let query = SELECT *
            FROM (SELECT f1 as j, SUM(f2) as sum
                  FROM t1 GROUP BY f1 WITH ROLLUP) as dt WHERE j+sum > 50 OR
            j IS NULL;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# Same, with a pre-existing condition in HAVING:

let query = SELECT *
            FROM (SELECT f1 as j, SUM(f2) as sum
                  FROM t1 GROUP BY f1 WITH ROLLUP
                  HAVING AVG(f2) > 1) AS dt WHERE j+sum > 50 OR
            j IS NULL;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# Test with ORDER BY. We should be able to push the condition
let query = SELECT f1 FROM (SELECT f1, f2 FROM t1 ORDER BY f2) as dt
            WHERE f1 > 3;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# Test with ORDER BY AND LIMIT. Now we should not be pushing the condition
# down as it will change the result for ORDER BY with LIMIT which changes the
# result of the derived table and there by changing the final result set
let query = SELECT f1 FROM (SELECT f1, f2 FROM t1 ORDER BY f2 LIMIT 4) as dt
            WHERE f1 > 0 ;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;
#Same as above
let query = SELECT f1 FROM (SELECT f1, f2 FROM t1 LIMIT 4) as dt WHERE f1 > 0;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
# Inherently a nondeterministic query, so depends on the query plan
eval $query;

# Test with WINDOW FUNCTIONS.
# ONLY_FULL_GROUP_BY flags some of these query.  However if f1 was a primary
# key these would be valid queres. So we switch off the mode, just for testing
# purpose.

# We cannot push past window function as we need the entire result set for
# window function to give correct results.  So the condition will not be
# pushed down to derived table.
set sql_mode="";
let query = SELECT * FROM (SELECT f1, SUM(f2) OVER() FROM t1 GROUP BY f1) as dt
            WHERE f1 > 2;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# We can push past window function as we partiiton on f1. It is further pushed
# past group by to the WHERE clause of the derived table.
let query = SELECT *
            FROM (SELECT f1, SUM(f2) OVER(PARTITION BY f1)
                  FROM t1 GROUP BY f1) as dt WHERE f1 > 2;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# We can push past window function as we partition on f2. But cannot push past
# group by. So pushed condition stays in the HAVING clause of the derived
let query = SELECT *
            FROM (SELECT f1, f2, SUM(f3) OVER(PARTITION BY f2)
                  FROM t1 GROUP BY f1) as dt WHERE f2 > 30;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# We can pushdown only a part of the condition to the derived table. "f2" is
# not part of the partition clause of window function
let query = SELECT *
            FROM (SELECT f1, f2, SUM(f3) OVER(PARTITION BY f1)
                  FROM t1 GROUP BY f1) as dt
            WHERE f1 > 2 and f2 > 30 and (f1+f2) > 40;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# We can push past window function partially and past group by partially here.
let query = SELECT *
            FROM (SELECT f1, f2, SUM(f3) OVER(PARTITION BY f1,f2)
                  FROM t1 GROUP BY f1) as dt
            WHERE f1 > 2 and f2 > 30 and (f1+f2) > 40;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# We can push past window function and group by for condition "f1 > 2". The
# other two conditions will stay in HAVING clause (Testing with expressions
# having fields from partition clause)
let query = SELECT *
            FROM (SELECT f1, f2, SUM(f3) OVER(PARTITION BY f1,f2),
                  AVG(f3) OVER (PARTITION BY f1)
                  FROM t1 GROUP BY f1) as dt
            WHERE f1 > 2 and f2 > 30 and (f1+f2) > 40;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# Test with partition on aggregation and condition having this aggregation in
# the condition.
let query = SELECT *
            FROM (SELECT f1, SUM(f2) as SUM, AVG(f3) OVER (PARTITION BY SUM(f2))
                  FROM t1 GROUP BY f1) as dt
            WHERE SUM > 40;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

#Test with multiple window functions having partition on fields in different
# order and a condition having one of the fields in the condition.
let query = SELECT *
            FROM (SELECT f1, SUM(f2) OVER (PARTITION by f1,f2),
                  AVG(f3) OVER (PARTITION BY f2,f1),
                  FIRST_VALUE(f3) OVER (PARTITION by f1)
                  FROM t1) as dt
            WHERE f1 > 2 ;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# Test with multiple window functions with different columns in partition.
# Should not push the condition down.
let query = SELECT *
            FROM (SELECT f1, SUM(f1) OVER (PARTITION by f2),
                  AVG(f2) OVER (PARTITION BY f1)
                  FROM t1) as dt
            WHERE f1 > 2 ;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;
set sql_mode=default;

# Test with parameters for prepared statements.
# Conditions having parameters can be pushed down.
let query = SELECT f1
            FROM (SELECT f1 FROM t1) as dt WHERE f1 > ?;
SET @p1 = 3;
eval PREPARE p FROM "$query";
eval EXECUTE p USING @p1;
eval PREPARE p FROM "EXPLAIN FORMAT=tree $query";
--replace_regex $elide_costs
eval EXECUTE p USING @p1;
DROP PREPARE p;

let query = SELECT l, m
            FROM (SELECT (i+3) as l, (j+4) as m
                  FROM (SELECT (f1+f2) AS i, (f3+?) AS j
                        FROM t1) AS dt1
                       ) AS dt2
            WHERE l > 20 and l+m > 10 ;
SET @p1 = 1;
eval PREPARE p FROM "EXPLAIN FORMAT=tree $query";
--replace_regex $elide_costs
eval EXECUTE p USING @p1;
eval PREPARE p FROM "$query";
eval EXECUTE p USING @p1;

# Test for not supported conditions.

# Test with non-deterministic expressions in column of derived
# table. These cannot be pushed down.
let query = SELECT * FROM (SELECT RAND() as a FROM t1) as dt
            WHERE a > 0.5;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;

# Test with non-deterministic expressions in conditions. These can be
# pushed down, but not so early that they would modify the order of
# operations in the computation of the derived table's content.

# RAND goes to HAVING, <10 goes to WHERE
let query = SELECT * FROM (SELECT f1, SUM(f2) FROM t1 GROUP BY f1) as dt
            WHERE f1 > 3*RAND() AND f1 < 10;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;

# RAND doesn't move, <10 goes to WHERE
let query = SELECT * FROM
            (SELECT f1, SUM(f2) OVER(PARTITION BY f1) FROM t1) as dt
            WHERE f1 > 3*RAND() AND f1 < 10;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;

# RAND and <10 go to WHERE
let query = SELECT * FROM
            (SELECT f1 FROM t1) as dt
            WHERE f1 > 3*RAND() AND f1<10;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;

# With WHERE condition having a subquery
let query = SELECT f1 FROM
(SELECT (SELECT 1 FROM t1 LIMIT 1) as f1 FROM t1) as dt WHERE f1 = 1;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# With WHERE condition having a stored routine: it is pushed down more
# if it is deterministic
DELIMITER |;
CREATE FUNCTION p() RETURNS INTEGER
BEGIN
  DECLARE retn INTEGER;
  SELECT count(f1) FROM t1 INTO retn;
  RETURN retn;
END|
DELIMITER ;|

let query = SELECT * FROM (SELECT f1, SUM(f2) FROM t1 GROUP BY f1) as dt
            WHERE p() = 1;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;
DROP FUNCTION p;

DELIMITER |;
CREATE FUNCTION p() RETURNS INTEGER DETERMINISTIC
BEGIN
  DECLARE retn INTEGER;
  SELECT count(f1) FROM t1 INTO retn;
  RETURN retn;
END|
DELIMITER ;|

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;
DROP FUNCTION p;

DELIMITER |;
CREATE PROCEDURE p()
BEGIN
  DECLARE val INTEGER;
  SET val = 2;
  EXPLAIN FORMAT=tree SELECT AVG(f1) FROM (SELECT * FROM t1) as dt
  WHERE f2 > val;
  SELECT AVG(f1) FROM (SELECT * FROM t1) as dt WHERE f2 > val;
END|
DELIMITER ;|

--replace_regex $elide_costs
CALL p();
DROP PROCEDURE p;

# Test with CTE's.  Condition pushdown to CTE's is allowed only if the
# underlying derived tables are not referenced multiple times.

# With this definition, we should be able to pushdown.
let query = SELECT * FROM ((WITH qn AS (SELECT 10*f1 as f1 FROM t1),
qn2 AS (SELECT 3*f1 AS f2 FROM qn)
SELECT * from qn2)) as dt WHERE f2 > 1;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;

# With this definition we cannot push the condition down. Note that qn is
# reference multiple times.
let query = SELECT * FROM ((WITH qn AS (SELECT 10*f1 as f1 FROM t1),
qn2 AS (SELECT 3*f1 AS f2 FROM qn)
SELECT * from qn,qn2)) as dt WHERE f1 < 10 and f2 > 1;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;


# With derived tables part of JOINS
let query = SELECT * FROM t1 JOIN (SELECT f1, f2 FROM t1) as dt USING (f2)
            WHERE dt.f1 > 31 and t1.f2  > 40;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;

# No pushdown as 'dt' is on the right side of a LEFT JOIN
let query = SELECT * FROM t1 LEFT JOIN (SELECT f1, f2 FROM t1) as dt USING (f2)
            WHERE dt.f1 is null;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;

# Pushdown of WHERE happens after conversion from LEFT JOIN to INNER
# JOIN in FROM, so it is possible here:
let query = SELECT * FROM t1 LEFT JOIN (SELECT f1, f2 FROM t1) as dt USING (f2)
            WHERE dt.f1 > 3;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;

let query = SELECT * FROM t1 INNER JOIN (SELECT f1, f2 FROM t1) as dt
            ON dt.f1 > 3;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;

# Alas the pushed condition cannot, inside the derived table,
# trigger a conversion to inner join, as simplify_joins() in the
# derived table is done first.

let query = SELECT * FROM t1 INNER JOIN(SELECT t2.f1, t2.f2 FROM t1
            LEFT JOIN t1 AS t2 ON TRUE) AS dt ON dt.f1 > 3;
--replace_regex $elide_costs
eval EXPLAIN FORMAT = tree $query;

# Test with both merge and derived combination
set optimizer_switch="derived_merge=on";
let query = SELECT * FROM (SELECT * FROM (SELECT f1, SUM(f2) AS sum
                                          FROM t1 GROUP BY f1) as dt1
                           WHERE f1 > 10) dt2 WHERE sum > 10;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;
let query = SELECT * FROM (SELECT f1, SUM(f2) AS sum
                           FROM (SELECT f1, f2 FROM t1 WHERE f1 > 10) as dt1
                           GROUP BY f1) dt2 WHERE sum > 10;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;

# Test when an inner derived table is merged

--sorted_result
SELECT * FROM
 (SELECT f1 FROM (SELECT f1 FROM t1) AS dt1 GROUP BY f1) AS dt2
 WHERE f1 > 3;

--sorted_result
SELECT * FROM
 (SELECT dt1.f1 FROM (SELECT f1 FROM t1) AS dt1, t1 AS t0
  GROUP BY dt1.f1) AS dt2
WHERE dt2.f1 > 3;

# Test with const conditions: shouldn't be pushed down (no benefit)
let query= SELECT /*+ no_merge(dt,dt1) */ * FROM
((SELECT f1, f2 FROM t1) as dt, (SELECT f1, f2 FROM t1) as dt1) WHERE FALSE;
--replace_column 10 #
--replace_regex $elide_costs
eval EXPLAIN $query;
--sorted_result
eval $query;

# Test for optimizer hints DERIVED_CONDIITON_PUSHDOWN and
# NO_DERIVED_CONDITION_PUSHDOWN

# Optimizer switch condition_pushdown_to_derived is ON. But hint will override
# the switch. So condition pushdown does not happen.
set optimizer_switch="derived_merge=off";
let query = SELECT /*+ NO_DERIVED_CONDITION_PUSHDOWN(dt2) */ * FROM
(SELECT * FROM (SELECT * FROM t1) as dt1) as dt2 WHERE f1 > 3;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;
let query = SELECT /*+ NO_DERIVED_CONDITION_PUSHDOWN() */ * FROM
(SELECT * FROM (SELECT * FROM t1) as dt1) as dt2 WHERE f1 > 3;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;
# Optimizer switch condition_pushdown_to_derived is OFF. So hints will dictate
# the behaviour.
set optimizer_switch="derived_condition_pushdown=off";
let query = SELECT /*+ DERIVED_CONDITION_PUSHDOWN(dt2) */ * FROM
(SELECT /*+ NO_DERIVED_CONDITION_PUSHDOWN(dt1) */ * FROM
 (SELECT * FROM t1) as dt1) as dt2 WHERE f1 > 3;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;
set optimizer_switch=default;

--echo # Bug#31491972: WL8084: SERVER CRASH FROM
--echo #               JOIN::UPDATE_SARGABLE_FROM_CONST()

--sorted_result
SELECT f1 FROM (SELECT DISTINCT * FROM t1 WHERE f2 = 4) AS alias1
WHERE ( alias1 . f1 = 24 AND alias1 . f3 = 101 );
--sorted_result
SELECT f1 FROM (SELECT DISTINCT * FROM t1 WHERE f2 = 4) AS alias1
WHERE ( alias1 . f1 BETWEEN 24 AND 30 AND alias1 . f3 BETWEEN 101 and 103);

DROP TABLE t0, t1;

--echo #
--echo # Bug#31603289:CRASH IN TABLE_LIST::GET_CLONE_FOR_DERIVED_EXPR,
--echo #              ASSERTION `FALSE' IN TABLE_LIST::GET_DERIVED_EXPR
--echo #

CREATE TABLE t(f0 INTEGER PRIMARY KEY, f1 INTEGER,f2 INTEGER);

--sorted_result
SELECT NULL IN(SELECT (f1 between 0 and 1)
 FROM (SELECT f1 FROM t WHERE  (@b:=NULL) - f2)as dt
);

DROP TABLE t;

--echo #
--echo # Bug#32127562:ERROR 3566 (HY000): ACCESS TO NATIVE FUNCTION IS
--echo #              REJECTED
--echo #

# Force optimizer to materialize the information schema table/view
set optimizer_switch="derived_merge=off";
# While making a copy of the condition that is pushed down to the materialized
# derived table, resolver should not throw error for conditions having fields
# from information schema tables invoking native functions.
SELECT 1 FROM information_schema.tables WHERE 12 IN (CONCAT_WS(TABLE_ROWS, ''));

set optimizer_switch="derived_merge=on";
--echo # BUG#32150145: 8.0.22+ RELEASE BUILD QUERY FAILS SILENTLY,
--echo #                DEBUG ASSERTION `THD->IS_ERROR()'
--echo #

CREATE TABLE t1(g INTEGER);

# For the following statement, condition "w.g is null" is pushed down to the
# derived table "w". In doing so, it should not crash because of a bad error
# state.
SELECT w.g FROM t1 INNER JOIN (
SELECT g, ROW_NUMBER() OVER (PARTITION BY g) AS r FROM t1
) w ON w.g=t1.g AND w.r=1 WHERE w.g IS NULL;

DROP TABLE t1;

--echo #
--echo # BUG#32863713: CTE CRASH IN QUERY_BLOCK::MASTER_QUERY_EXPRESSION
--echo #

CREATE TABLE t(f1 INTEGER);
# Checking if condition pushdown to a derived table does not
# crash the server when one of derived table is merged.
--replace_regex $elide_costs
EXPLAIN SELECT a1, a2
 FROM (SELECT MAX(2) AS a1 FROM t) as dt1,
 (SELECT @a AS a2 FROM t) as dt2
 WHERE dt1.a1 <= dt2.a2;

SELECT a1, a2
 FROM (SELECT MAX(f1) AS a1 FROM t) as dt1,
 (SELECT @a AS a2 FROM t) as dt2
 WHERE dt1.a1 <= dt2.a2;

DROP TABLE t;

--echo #
--echo # Bug#32905044: CRASH AT CONDITION_PUSHDOWN::REPLACE_COLUMNS_IN_COND
--echo #	       DURIN RQG CONCURRENCY RUNS

CREATE TABLE t(f1 INTEGER);
CREATE ALGORITHM=temptable VIEW v AS SELECT f1 FROM t;

#Query should not crash
SELECT f1 FROM (SELECT f1 FROM v) AS dt1 NATURAL JOIN v dt2 WHERE f1 > 5;

DROP TABLE t;
DROP VIEW v;

--echo #
--echo # Bug#32959186: DERIVED CONDITION PUSHDOWN IS NOT AVAILABLE FOR
--echo #               INSERT ... SELECT QUERIES

CREATE TABLE t1(f1 INTEGER, KEY(f1));
CREATE TABLE t2(f1 INTEGER);
INSERT INTO t1 VALUES (1),(2),(3),(4),(5);
CREATE ALGORITHM=temptable VIEW v AS SELECT f1 FROM t1;
ANALYZE TABLE t1;

#Check that condition f1=2 is pushed down to "v"
--replace_regex $elide_costs
EXPLAIN format=tree INSERT INTO t2 SELECT * FROM v WHERE f1=2;
INSERT INTO t2 SELECT * FROM v WHERE f1=2;
#Check that condition f1=2 is pushed down to "v"
--replace_regex $elide_costs
EXPLAIN format=tree UPDATE t2 SET f1=3 WHERE f1 IN (SELECT f1 FROM v WHERE f1=2);
UPDATE t2 SET f1=3 WHERE f1 IN (SELECT f1 FROM v WHERE f1=2);
#Check that condition f1=3 is pushed down to "v"
--replace_regex $elide_costs
EXPLAIN format=tree DELETE FROM t2 WHERE f1 IN (SELECT f1 FROM v WHERE f1=3);
DELETE FROM t2 WHERE f1 IN (SELECT f1 FROM v WHERE f1=3);

DROP TABLE t1;
DROP TABLE t2;
DROp VIEW v;

--echo #
--echo # Bug#33341080: Derived condition pushdown rewrite ignores user variables
--echo #

CREATE TABLE t1(f1 INTEGER);
INSERT INTO t1 VALUES (100),(200),(300),(400),(500);
ANALYZE TABLE t1;

let query = SELECT *
            FROM (SELECT f1, (@rownum_r := @rownum_r + 1) AS r FROM t1) AS dt
            WHERE dt.f1 = 300;

SET @rownum_r=0;
--replace_regex $elide_costs
# This should not push the condition down to derived table because of user
# variable assignments.
eval EXPLAIN FORMAT=tree $query;
SET @rownum_r=0;
eval $query;

DROP TABLE t1;

# Tests for WL#13730 - Condition pushdown to derived table with unions.
# WL#349 adds INTERSECT and EXCEPT: added some tests also for those
# set operators.

#setup tables
CREATE TABLE t0 (
  i0 INTEGER
);

INSERT INTO t0 VALUES (0),(1),(2),(3),(4);

CREATE TABLE t1 (f1 INTEGER, f2 INTEGER, f3 INTEGER,
                 KEY(f1), KEY(f1,f2), KEY(f3));
INSERT INTO t1
SELECT i0, i0 + 10*i0,
       i0 + 10*i0 + 100*i0
FROM t0 AS a0;
INSERT INTO t1
SELECT i0, i0 + 10*i0,
       i0 + 10*i0 + 100*i0
FROM t0 AS a0;
INSERT INTO t1 VALUES (NULL, 1, 2);
INSERT INTO t1 VALUES (NULL, 1, 3);

CREATE TABLE t2 LIKE t1;
INSERT INTO t2 SELECT * FROM t1;

ANALYZE TABLE t1,t2;

# Test with simple WHERE condition that needs to be pushed
let query = SELECT *
            FROM (SELECT f1, f2 FROM t1
		  UNION
		  SELECT f1, f2 FROM t2) as dt
	    WHERE f1 > 2;
# Masking all cost output as it makes the test unstable -
# most likely because they differ with innodb_page_size
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;

# Same, with INTERSECT
let query = SELECT *
            FROM (SELECT f1, f2 FROM t1
                  INTERSECT
                  SELECT f1, f2 FROM t2) as dt
            WHERE f1 > 2;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;

# Same, with EXCEPT
let query = SELECT *
            FROM (SELECT f1, f2 FROM t1
                  EXCEPT
                  SELECT f1, f2 FROM t2) as dt
            WHERE f1 > 2;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;

# Test with nested derived tables
let query = SELECT *
            FROM (SELECT * FROM (SELECT * FROM t1
                                 UNION
                                 SELECT * FROM t1) as dt1
                  UNION
                  SELECT * FROM t2) as dt2
            WHERE (f1+f2) > 22;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# Same, with INTERSECT
let query = SELECT *
            FROM (SELECT * FROM (SELECT * FROM t1
                                 INTERSECT
                                 SELECT * FROM t1) as dt1
                  INTERSECT
                  SELECT * FROM t2) as dt2
            WHERE (f1+f2) > 22;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;

# Nested derived table with fields that have different aliases
let query = SELECT i, j, k
            FROM (SELECT f1 as i, f2 as j, f3 as k FROM t1
                  UNION
                  SELECT f1 as l, f2 as m, f3 as n FROM t2) as dt
            WHERE i > 1 and i+j > 40 and k < 500;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# With expressions in the underlying derived tables.
let query = SELECT i, j
            FROM (SELECT f1+f2 as i, f2+f3 as j FROM t1
                  UNION
                  SELECT f1 as l, f2+f3 as m FROM t2) as dt
            WHERE i > 30 and i+j > 300;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;

# Same, with INTERSECT
let query = SELECT i, j
            FROM (SELECT f1+f2 as i, f2+f3 as j FROM t1
                  INTERSECT
                  SELECT f1 as l, f2+f3 as m FROM t2) as dt
            WHERE i > 30 and i+j > 300;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;

# Test with projection list for derived tables
let query = SELECT i
            FROM (SELECT f1 FROM t1
                  UNION
                  SELECT f2 FROM t2) as dt(i)
            WHERE i > 10;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# Test with aggregated query
let query = SELECT *
            FROM (SELECT f1, SUM(f2) FROM t1 GROUP BY f1
                  UNION
                  SELECT f1, f2 FROM t1) as dt
            WHERE f1 > 3;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;

# Same, with a pre-existing condition in HAVING:
let query = SELECT *
            FROM (SELECT f1 as j, SUM(f2) as sum
                  FROM t1 GROUP BY f1 WITH ROLLUP
                  HAVING AVG(f2) > 1
                  UNION ALL
                  SELECT f1 as j, SUM(f2) as sum
                  FROM t1 GROUP BY f1) AS dt
            WHERE j+sum > 50 OR j IS NULL;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# Test with ORDER BY. We should be able to push the condition
let query = SELECT f1
            FROM (SELECT f1, f2 FROM t1
                  UNION
                  SELECT f1, f2 FROM t2 ORDER BY f2) as dt
            WHERE f1 > 3;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# Test with WINDOW FUNCTIONS.
# We can push past window function and group by.
let query = SELECT *
            FROM (SELECT f1, f2, SUM(f2) OVER(PARTITION BY f1,f2)
                  FROM t1 GROUP BY f1, f2
                  UNION ALL
                  SELECT f1, f2, SUM(f2) OVER(PARTITION BY f1,f2)
                  FROM t2 GROUP BY f1,f2) as dt
            WHERE f1 > 2 and f2 > 30 and (f1+f2) > 40;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# Same, with INTERSECT
let query = SELECT *
            FROM (SELECT f1, f2, SUM(f2) OVER(PARTITION BY f1,f2)
                  FROM t1 GROUP BY f1, f2
                  INTERSECT ALL
                  SELECT f1, f2, SUM(f2) OVER(PARTITION BY f1,f2)
                  FROM t2 GROUP BY f1,f2) as dt
            WHERE f1 > 2 and f2 > 30 and (f1+f2) > 40;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

set sql_mode=default;

# Test with parameters. Pushdown is allowed for conditions having
# parameters.
let query = SELECT f1
            FROM (SELECT f1 FROM t1
                  UNION
                  SELECT f1 FROM t1) as dt WHERE f1 > ?;
SET @p1 = 3;
eval PREPARE p FROM "$query";
eval EXECUTE p USING @p1;
eval PREPARE p FROM "EXPLAIN FORMAT=tree $query";
--replace_regex $elide_costs
eval EXECUTE p USING @p1;
DROP PREPARE p;

let query = SELECT l,m
            FROM (SELECT (i+10+?) as l, (j+11+?) as m
                  FROM (SELECT (f1+f2) as i, (f3+?) as j
                        FROM t1 UNION
                        SELECT (f1+f3) as i, (f2+?) as j
                        FROM t2) as dt1
                  GROUP BY l,m) as dt2
            WHERE l > 20 and ?-m+? > 10;
SET @p1=1, @p2=2, @p3=3, @p4=4, @p5=5, @p6=6;
eval PREPARE p FROM "EXPLAIN FORMAT=tree $query";
--replace_regex $elide_costs
eval EXECUTE p USING @p1, @p2, @p3, @p4, @p5, @p6;
eval PREPARE p FROM "$query";
eval EXECUTE p USING @p1, @p2, @p3, @p4, @p5, @p7;

# Test for not supported conditions.
# Test with non-deterministic expressions in column of derived
# table. These cannot be pushed down.
let query = SELECT *
            FROM (SELECT f1 as a FROM t1
                  UNION ALL
                  SELECT RAND() as a FROM t1) as dt
            WHERE a > 0.5;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;

# Having non-deterministic expressions in the where condition
# cannot be pushed down if derived table has unions (without
# unions we do).
let query = SELECT *
            FROM (SELECT f1, SUM(f2) FROM t1 GROUP BY f1
                  UNION ALL
                  SELECT f1, f2 FROM t1) as dt
            WHERE f1 > 3*RAND() AND f1 < 10;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;

# Same, with INTERSECT
let query = SELECT *
            FROM (SELECT f1, SUM(f2) FROM t1 GROUP BY f1
                  INTERSECT ALL
                  SELECT f1, f2 FROM t1) as dt
            WHERE f1 > 3*RAND() AND f1 < 10;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;

# With WHERE condition having a subquery. This cannot be pushed
# down.
let query = SELECT f1
            FROM (SELECT f1 FROM t1
                  UNION ALL
                  SELECT (SELECT 1 FROM t1 LIMIT 1) as f1 FROM t1) as dt
            WHERE f1 = 1;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

# With query expression having LIMIT. This cannot be pushed down.
let query = SELECT f1
            FROM (SELECT f1 FROM t1
                  UNION ALL
                  SELECT f1 FROM t1 LIMIT 1) as dt
            WHERE f1 = 1;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
let query = SELECT f1
            FROM (SELECT f1 FROM t1
                  UNION ALL
                  (SELECT f1 FROM t1 LIMIT 1)) as dt
            WHERE f1 = 1;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;

# With WHERE condition referencing a stored function: it is pushed down
# if it is deterministic.
DELIMITER |;
CREATE FUNCTION p() RETURNS INTEGER
BEGIN
  DECLARE retn INTEGER;
  SELECT count(f1) FROM t1 INTO retn;
  RETURN retn;
END|
DELIMITER ;|

let query = SELECT *
            FROM (SELECT f1, SUM(f2) FROM t1 GROUP BY f1
                  UNION
                  SELECT f1, f2 FROM t1) as dt
            WHERE p() = 1;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;
DROP FUNCTION p;

DELIMITER |;
CREATE FUNCTION p() RETURNS INTEGER DETERMINISTIC
BEGIN
  DECLARE retn INTEGER;
  SELECT 1 INTO retn;
  RETURN retn;
END|
DELIMITER ;|

--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;
DROP FUNCTION p;

# Conditions having SP variables can be pushed down
DELIMITER |;
CREATE PROCEDURE p()
BEGIN
  DECLARE val INTEGER;
  SET val = 2;
  EXPLAIN FORMAT=tree SELECT AVG(f1)
                      FROM (SELECT * FROM t1
                            UNION
                            SELECT * FROM t1) as dt
                      WHERE f2 > val;
  SELECT AVG(f1)
  FROM (SELECT * FROM t1
        UNION
        SELECT * FROM t1) as dt
  WHERE f2 > val;
END|
DELIMITER ;|

--replace_regex $elide_costs
CALL p();
DROP PROCEDURE p;

# Test with CTE's.  Condition pushdown to recursive CTE's is not allowed

let query = WITH recursive qn AS
              (SELECT 10*f1 as f1 FROM t1
               UNION
               SELECT * FROM qn) SELECT * FROM qn WHERE f1 > 1;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;

# With derived tables part of JOINS
# Should be able to pushdown dt.f1 > 31 to derived table dt
let query = SELECT *
            FROM t1 JOIN (SELECT f1, f2 FROM t1
                          UNION SELECT f1, f2 FROM t1) as dt USING (f2)
            WHERE dt.f1 > 31 and t1.f2  > 40;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;

# Same, with INTERSECT
let query = SELECT *
            FROM t1 JOIN (SELECT f1, f2 FROM t1
                          EXCEPT SELECT f1, f2 FROM t1) as dt USING (f2)
            WHERE dt.f1 > 31 and t1.f2  > 40;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;

# Test with both merged and materialized combination
let query = SELECT *
            FROM (SELECT *
                  FROM (SELECT f1, f2 FROM t1
                        UNION
                        SELECT f1, f2 FROM t1) as dt1
                  WHERE f1 > 10) dt2
            WHERE f2 > 30;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;

let query = SELECT *
            FROM (SELECT f1, f2
                  FROM (SELECT f1+f2 as f1, f2 FROM t1) as dt1
                  UNION
                  SELECT f1, f2 FROM t1) as dt2
            WHERE f1 > 30;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
--sorted_result
eval $query;

#Check for insert/update/delete
CREATE ALGORITHM=temptable VIEW v AS (SELECT f1,f2,f3 FROM t1
                                      UNION
                                      SELECT f1,f2,f3 FROM t1);
#Check that condition f1=2 is pushed down to "v"
--replace_regex $elide_costs
EXPLAIN format=tree INSERT INTO t2 SELECT * FROM v WHERE f1=2;
INSERT INTO t2 SELECT * FROM v WHERE f1=2;
#Check that condition f1=2 is pushed down to "v"
--replace_regex $elide_costs
EXPLAIN format=tree UPDATE t2 SET f1=3 WHERE f1 IN (SELECT f1 FROM v WHERE f1=2);
UPDATE t2 SET f1=3 WHERE f1 IN (SELECT f1 FROM v WHERE f1=2);
#With multi-table update
--replace_regex $elide_costs
EXPLAIN format=tree UPDATE t2 JOIN v ON t2.f2=v.f2
                    SET t2.f1 = t2.f1 + v.f1
                    WHERE v.f2 > 10;
UPDATE t2 JOIN v ON t2.f2=v.f2 SET t2.f1 = t2.f1 + v.f1 WHERE v.f2 > 10;
#Check that condition f1=3 is pushed down to "v"
--replace_regex $elide_costs
EXPLAIN format=tree DELETE FROM t2 WHERE f1 IN (SELECT f1 FROM v WHERE f1=3);
DELETE FROM t2 WHERE f1 IN (SELECT f1 FROM v WHERE f1=3);
#With multi-table delete 
--replace_regex $elide_costs
EXPLAIN format=tree DELETE t2 FROM t2 JOIN v ON t2.f2=v.f2 WHERE v.f2 > 10;
DELETE t2 FROM t2 JOIN v ON t2.f2=v.f2 WHERE v.f2 > 10;

DROP VIEW v;
DROP TABLE t0;
DROP TABLE t1;
DROP TABLE t2;

# Code coverage
# Test the need for considering hidden fields added (while setting up
# temporary table for a materialized derived table) to correectly get
# the position of a derived table expression in its query block.

CREATE TABLE t1(f1 VARBINARY(10000));

SELECT * FROM (SELECT f1 FROM t1 UNION SELECT f1 FROM t1) as dt WHERE f1 > '10';

DROP TABLE t1;

--echo #
--echo # Bug#33791802: Query with multiple JOINs stopped working
--echo #

CREATE TABLE t1(f1 INTEGER, f2 INTEGER);
CREATE VIEW v1 AS SELECT * FROM t1 UNION SELECT * FROM t1;
CREATE VIEW v2 AS SELECT * FROM v1;

# The following query should not error out while resolving the
# field "v2.f2" during condition pushdown.
# The natural join changes the context for the fields to point
# to the tables in the join list. During simplification of joins,
# the join list starts pointing to the underlying tables of the view
# v2 since this view was merged. This made the resolving of field
# v2.f2 error out since it tried to find the field in view v1.
# We now use the expression in the underlying table of a merged view
# for cloning during condition pushdown.
SELECT t1.f1 FROM t1 JOIN v2 USING(f2) WHERE v2.f2 = 1;

DROP TABLE t1;
DROP VIEW v1;
DROP VIEW v2;

# Original query from bugpage
SELECT a.table_name, d.table_name
FROM information_schema.key_column_usage a
     JOIN information_schema.table_constraints b
     USING (table_schema , table_name , constraint_name)
     JOIN information_schema.referential_constraints c
     ON (c.constraint_name = b.constraint_name AND
         c.table_name = b.table_name AND
         c.constraint_schema = b.table_schema)
     LEFT JOIN information_schema.table_constraints d
     ON (a.referenced_table_schema = d.table_schema AND
         a.referenced_table_name = d.table_name AND
         d.constraint_type IN ('UNIQUE' , 'PRIMARY KEY'))
WHERE b.constraint_type = 'FOREIGN KEY'
ORDER BY a.table_name , a.ordinal_position;

# Simplified query
SELECT a.table_name
FROM information_schema.key_column_usage a
     JOIN information_schema.table_constraints b
     USING (table_schema)
WHERE b.constraint_type = 'FOREIGN KEY';

#End of test for Bug#33791802

--echo #
--echo # Bug#33838439: ExtractValue not working properly with COUNT
--echo #

CREATE TABLE t1 (f1 INTEGER);

SET @a = 0;
--replace_regex $elide_costs
EXPLAIN SELECT COUNT(*) FROM (SELECT SUM(f1) FROM t1) as dt WHERE @a = 1;
SELECT COUNT(*) FROM (SELECT SUM(f1) FROM t1) as dt WHERE @a = 1;

DROP TABLE t1;

--echo #
--echo # Bug#34148712: Incorrect DATETIME value: '' on latest MySQL 8.0.29
--echo #

CREATE TABLE t1 (f1 INTEGER UNSIGNED);
let $query = SELECT *
             FROM (SELECT NOW() AS time FROM t1 WHERE f1 = ?
                   UNION
                   SELECT NOW() AS time FROM t1 WHERE f1 = 0) AS dt
             WHERE time <= ?;
eval PREPARE stmt FROM "$query";
SET @a = 1;
SET @b = '2022-05-06 16:49:45';
eval EXECUTE stmt USING @a, @b;
eval PREPARE stmt FROM "EXPLAIN FORMAT=tree $query";
--replace_regex $elide_costs
eval EXECUTE stmt USING @a, @b;
SET @a = 2;
SET @b = '2023-05-06 16:49:45';
SET timestamp=UNIX_TIMESTAMP('2023-05-06 16:49:45');
--replace_regex $elide_costs
eval EXECUTE stmt USING @a, @b;
SET timestamp=default;

DROP TABLE t1;

--echo #
--echo # Bug#34298238: Assertion `cond->is_bool_func()' failed in MySQL 8.0.29
--echo #

CREATE TABLE t1 (f1 INTEGER);
INSERT INTO t1 VALUES (1);
SELECT * FROM (SELECT f1 FROM t1 UNION SELECT f1 FROM t1) AS dt WHERE f1 <> 0.5;
DROP TABLE t1;

--echo #
--echo # Bug#34515868: Temptable views on information_schema fails on filters
--echo #

# We should not see "Access denied" error when the where condition
# is pushed down to views referencing a system view.

CREATE ALGORITHM=TEMPTABLE VIEW v1 AS
 SELECT VIEW_DEFINITION FROM INFORMATION_SCHEMA.VIEWS;
SELECT * FROM v1 WHERE VIEW_DEFINITION LIKE 'x';

CREATE ALGORITHM=TEMPTABLE VIEW v2 AS SELECT * FROM v1;
SELECT * FROM v2 WHERE VIEW_DEFINITION LIKE 'x';

CREATE ALGORITHM=TEMPTABLE VIEW v3 AS
 SELECT * FROM (SELECT * FROM v1 UNION SELECT * FROM v2) AS dt;
SELECT * FROM v3 WHERE VIEW_DEFINITION LIKE 'x';

DROP VIEW v1, v2, v3;

--echo # Bug#34359297: Incorrect work of query with union in inner select

CREATE TABLE t1 (
  str VARCHAR(200) CHARACTER SET utf16 COLLATE utf16_unicode_ci
) ENGINE=InnoDB DEFAULT CHARACTER SET ascii COLLATE ascii_general_ci;

CREATE TABLE t2 (
  str VARCHAR(200) CHARACTER SET utf16 COLLATE utf16_unicode_ci
) ENGINE=InnoDB DEFAULT CHARACTER SET ascii COLLATE ascii_general_ci;

INSERT INTO t1 VALUES (_utf8mb4'Patch');

ANALYZE TABLE t1,t2;

let $query =
SELECT dt.str
FROM (SELECT t2.str
      FROM t2
      WHERE t2.str LIKE _latin1'Patc%' ESCAPE _latin1'\\\\'
      UNION ALL
      SELECT t1.str
      FROM t1
      WHERE t1.str LIKE _latin1'Patc%' ESCAPE _latin1'\\\\'
     ) AS dt
WHERE dt.str LIKE _latin1'P%' ESCAPE _latin1'\\\\';
--replace_regex $elide_costs_and_rows
eval explain format=tree $query;
eval $query;

DROP TABLE t1, t2;

--echo #
--echo # Bug#34781533: Result set is incorrect
--echo #

CREATE TABLE t1 (f1 INTEGER, f2 INTEGER);
INSERT INTO t1 VALUES (NULL, NULL);

SELECT 1
FROM (SELECT 1
      FROM t1 LEFT JOIN (SELECT 1 AS f1 FROM t1) AS dt1
      ON dt1.f1 = t1.f2
      WHERE dt1.f1 IS NOT NULL) AS dt2,
     (SELECT 1 FROM t1 UNION ALL SELECT 2 FROM t1) AS dt3;

DROP TABLE t1;

--echo #
--echo # Bug#34919799: Assertion `cond->is_bool_func()' failed.
--echo #

CREATE TABLE t1 (f1 TINYINT);

SELECT f1 FROM (SELECT f1 FROM t1 UNION SELECT f1 FROM t1 ) AS dt
WHERE f1 > -32768 OR f1 = 1;

DROP TABLE t1;

--echo #
--echo # Bug#34996488: utf8mb4 identifiers stopped working since
--echo #               8.0.32 release
--echo #

SELECT * FROM (SELECT 'å' AS x) AS dt WHERE x = 'å';

CREATE TABLE t1 (f1 VARCHAR(10));
INSERT INTO t1 VALUES('å');

SELECT * FROM (SELECT f1 FROM t1 UNION SELECT f1 FROM t1) AS dt WHERE f1 = 'å';

DROP TABLE t1;

--echo #
--echo # Bug#35102220: Assertion failure in Item::walk_helper_thunk
--echo #               at ../sql/item.h
--echo #

CREATE TABLE t1 (f1 INTEGER);
SELECT f1
FROM t1 JOIN
     LATERAL (SELECT (t1.f1 + t2.f1) AS f2
              FROM t1 AS t2
              GROUP BY f2) AS dt
WHERE f2 = 9;

DROP TABLE t1;

--echo # Bug#35211828: Derived condition pushdown with rollup gives wrong
--echo #               results
--echo #

CREATE TABLE t1 (f1 INTEGER, f2 INTEGER);
INSERT INTO t1 VALUES(1,2);
INSERT INTO t1 VALUES (2,2);

SELECT f1, f2 FROM (SELECT f1, f2 FROM t1 GROUP BY f1, f2 WITH ROLLUP) as dt
WHERE f2 IS NULL;

DROP TABLE t1;

--echo #
--echo # Bug#35634714: Derived condition pushdown return wrong results
--echo #

CREATE TABLE t1(f1 INTEGER);
INSERT INTO t1 VALUES (NULL);
CREATE TABLE t2(f1 INTEGER);

SELECT dt3.f1
FROM t1 JOIN (SELECT f1
              FROM (SELECT dt1.f1
                    FROM t1
                         LEFT JOIN (SELECT 1 AS f1
                                    FROM t2) AS dt1
                         ON TRUE) AS dt2
              GROUP BY f1) AS dt3
WHERE dt3.f1 IS NOT NULL;

DROP TABLE t1,t2;

--echo #
--echo # Bug#35689804: Depending on the data inserted, the results of
--echo #               the PREPARE and the general query run are different.
--echo #

CREATE TABLE t1 (f1 INTEGER, f2 VARCHAR(30) COLLATE utf8mb4_bin NOT NULL);
INSERT INTO t1(f2) VALUES ('680519363848');

# Query from the bug report
let query =
SELECT *
FROM (SELECT IF(f2 = ?, ?, CASE WHEN f2 IS NULL THEN ? ELSE ? END) AS case_f
      FROM t1
      UNION ALL
      SELECT IF(f2 = ?, ?, CASE WHEN f2 IS NULL THEN ? ELSE ? END) AS case_f
      FROM t1) AS dt1
WHERE case_f = ?;

SET @a1 = 'Y';
SET @a2 = 'Y';
SET @a3 = 'N';
SET @a4 = 'Y';
SET @a5 = 'Y';
SET @a6 = 'Y';
SET @a7 = 'N';
SET @a8 = 'Y';
SET @a9 = 'Y';

eval PREPARE stmt FROM "$query";
eval EXECUTE stmt USING @a1, @a2, @a3, @a4, @a5, @a6, @a7, @a8, @a9;
eval PREPARE stmt FROM "EXPLAIN FORMAT=tree $query";
--replace_regex $elide_costs
eval EXECUTE stmt USING @a1, @a2, @a3, @a4, @a5, @a6, @a7, @a8, @a9;

# Simplified query
SET optimizer_switch="derived_merge=off";
let query =
SELECT * FROM (SELECT ? AS case_f FROM t1) as dt1 WHERE case_f = 'Y';
SET @a1 = 'Y';
eval PREPARE stmt FROM "$query";
eval EXECUTE stmt USING @a1;
eval PREPARE stmt FROM "EXPLAIN FORMAT=tree $query";
--replace_regex $elide_costs
eval EXECUTE stmt USING @a1;

SET optimizer_switch=default;

DROP TABLE t1;

--echo #
--echo # Bug#35671595: Prepared statement with CTE assertion error
--echo #

PREPARE stmt FROM
 'WITH t1(f1) AS
   (WITH t2(f1) AS
     (SELECT ?) SELECT 1 FROM t2 WHERE t2.f1)
   SELECT 1 FROM t1 JOIN t1 AS t3';

SET @a = 1;
EXECUTE stmt USING @a;

PREPARE stmt FROM
 'WITH t1(f1) AS
   (WITH t2(f1) AS
     (SELECT ?+?) SELECT 1 FROM t2 WHERE t2.f1)
   SELECT 1 FROM t1 JOIN t1 AS t3';

SET @a = 1;
SET @b = 2;
EXECUTE stmt USING @a, @b;

--echo #
--echo # Bug#35846847: Assertion Failure in
--echo #               /mysql-8.0.34/sql/sql_derived.cc:524
--echo #

CREATE TABLE t1 (f1 integer);

let $query = SELECT
             (WITH cte AS (SELECT f1 AS f2 FROM t1)
              SELECT f1
              FROM (SELECT f1 FROM t1
                    EXCEPT
                    SELECT t2.f1 FROM cte) AS dt
              WHERE f1 > 0) AS f3
             FROM t1, t1 AS t2
             GROUP BY f3;
--replace_regex $elide_costs
eval EXPLAIN format=tree $query;
eval $query;

DROP TABLE t1;

--echo #
--echo # Bug#36246859: Collation issue: ERROR 1253 (42000):
--echo #               COLLATION '' is not valid for CHARACTER SET
--echo #

CREATE TABLE t1 (f1 char(10)
CHARACTER SET latin1 COLLATE latin1_spanish_ci DEFAULT NULL)
DEFAULT CHARSET=latin1 COLLATE=latin1_spanish_ci;
INSERT INTO t1 VALUES ('C');
INSERT INTO t1 VALUES ('D');

# Set charset to latin for creating the view.
SET character_set_client = latin1;
SET character_set_connection = latin1;
CREATE VIEW v1 AS
SELECT 'A' COLLATE latin1_spanish_ci AS field1 FROM t1
UNION
SELECT 'B' COLLATE latin1_spanish_ci AS field1 FROM t1 AS t2;

# Use a different charset now to query the view.
SET character_set_connection = default;
# Condition pushdown to view "v1" should happen
# without any errors as it now uses the view's
# connection context when "field1" is replaced
# with "'A' COLLATE latin1_spanish_ci".
let $query = SELECT * FROM v1 WHERE field1 IN ('C');
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;
let $query = SELECT * FROM (SELECT * FROM v1 UNION SELECT * FROM v1) as dt
            WHERE field1 IN ('A');
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;
# view context of "v1" is not used when cloning the condition
# "field1 IN ('C') OR COALESCE('A' COLLATE utf8mb4_spanish_ci IN ('B'), field1)"
# to be pushed down in case of SET operations. It is used only when
# "field1" is replaced with "'A' COLLATE latin1_spanish_ci".
let $query = SELECT * FROM v1
             WHERE field1 IN ('C') OR
                   COALESCE('A' COLLATE utf8mb4_spanish_ci IN ('B'), field1);
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;
DROP VIEW v1;

#Simplified repro
# Set charset to latin for creating the view.
SET character_set_client = latin1;
SET character_set_connection = latin1;
CREATE VIEW v1 AS SELECT 'A' COLLATE latin1_spanish_ci AS field1 FROM t1;

# Use a different charset now to query the view.
SET character_set_connection = default;
# We do not want the view to be merged. So set "derived_merge" should be off.
SET optimizer_switch="derived_merge=off";
let $query = SELECT * FROM v1 WHERE field1 IN ('C');
--replace_regex $elide_costs
eval EXPLAIN FORMAT=tree $query;
eval $query;

SET optimizer_switch=default;
SET character_set_client = default;

DROP VIEW v1;
DROP TABLE t1;

--echo #
--echo # Bug#36368181: Assertion failure in operator() at sql/sql_derived.cc
--echo #
CREATE TABLE t1 (f1 INTEGER);
# dt2.field1 which is used in condition pushdown has an expression which
# involves one outer reference "dt1.f1" which is also a view reference
# (since dt1 gets merged) and a column which is not a outer reference
# "t1.f1". This would earlier fail some checks during cloning.
SELECT *
FROM (SELECT * FROM t1) AS dt1
WHERE (SELECT 1
       FROM (SELECT (dt1.f1+t1.f1) AS field1
             FROM t1
             GROUP BY field1) dt2
       WHERE dt2.field1 = 1);
DROP TABLE t1;
