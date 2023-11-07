--source ./elide_costs.inc

--echo #
--echo # WL#15588 Store EXPLAIN FORMAT=JSON SELECT output in user variable.
--echo #

--source include/elide_costs.inc

SET @v1 = 'UNCHANGED';
SET @v2 = @v1;

--echo
--echo # Unsupported functionality.
--error ER_EXPLAIN_INTO_IMPLICIT_FORMAT_NOT_SUPPORTED
EXPLAIN INTO @v1 SELECT 1;
--error ER_EXPLAIN_INTO_FORMAT_NOT_SUPPORTED
EXPLAIN FORMAT=TRADITIONAL INTO @v1 SELECT 1;
--error ER_EXPLAIN_INTO_FORMAT_NOT_SUPPORTED
EXPLAIN FORMAT=TREE INTO @v1 SELECT 1;
--error ER_EXPLAIN_INTO_FOR_CONNECTION_NOT_SUPPORTED
EXPLAIN FORMAT=JSON INTO @v1 FOR CONNECTION 1;
--error ER_ILLEGAL_USER_VAR
EXPLAIN FORMAT=JSON INTO @ SELECT 1;
--error ER_ILLEGAL_USER_VAR
EXPLAIN FORMAT=JSON INTO @'' SELECT 1;

--echo # Check that the variable was not changed on error.
SELECT @v1, @v1 = @v2;

--echo
CREATE TABLE t1 (i1 INT PRIMARY KEY, i2 INT);
CREATE TABLE t2 (i3 INT, i4 INT);

INSERT INTO t1 VALUES (1,2), (2,3), (3,4);
INSERT INTO t2 SELECT i2, i1 FROM t1;
ANALYZE TABLE t1, t2;

# If the EXPLAIN output of a simple version of each different type of expression
# is successfully stored in the user variable, and all other regular EXPLAIN
# tests work as expected, we can infer that storing the result of more complex
# EXPLAIN statements will also work.

--echo
--echo # EXPLAIN SELECT.
EXPLAIN FORMAT=JSON INTO @v1 SELECT * FROM t1 JOIN t2 ON i1 = i3 WHERE i2 = 2;
--replace_regex $elide_json_costs
SELECT @v1, JSON_VALID(@v1);

--echo
--echo # User variable is overwritten on successful execution.
SET @v2 = @v1;
EXPLAIN FORMAT=JSON INTO @v1 SELECT * FROM t1;
SELECT JSON_VALID(@v1), @v1 != @v2, @v1 NOT LIKE CONCAT(@v2, '%');

SET @v1 = NULL;

--echo
--echo # EXPLAIN single table INSERT VALUES.
EXPLAIN FORMAT=JSON INTO @v1 INSERT INTO t1 VALUES (4,5);
SELECT JSON_VALID(@v1), COUNT(*) = 3 FROM t1;

SET @v1 = NULL;

--echo
--echo # EXPLAIN single table INSERT SELECT.
EXPLAIN FORMAT=JSON INTO @v1 INSERT INTO t1 SELECT * FROM t2;
SELECT JSON_VALID(@v1), COUNT(*) = 3 FROM t1;

SET @v1 = NULL;

--echo
--echo # EXPLAIN single table UPDATE.
EXPLAIN FORMAT=JSON INTO @v1 UPDATE t1 SET i2 = i2 + 1 WHERE i1 = 1;
SELECT JSON_VALID(@v1), i2 = 2 FROM t1 WHERE i1 = 1;

SET @v1 = NULL;

--echo
--echo # EXPLAIN single table REPLACE.
EXPLAIN FORMAT=JSON INTO @v1 REPLACE t1 SELECT * FROM t2;
SELECT JSON_VALID(@v1), COUNT(*) = 3 FROM t1;

SET @v1 = NULL;

--echo
--echo # EXPLAIN single table DELETE.
EXPLAIN FORMAT=JSON INTO @v1 DELETE FROM t1;
SELECT JSON_VALID(@v1), COUNT(*) = 3 FROM t1;

SET @v1 = NULL;

--echo
--echo # EXPLAIN multitable UPDATE.
EXPLAIN FORMAT=JSON INTO @v1 UPDATE t1, t2 SET i1 = i1 - 1, i3 = i3 + 1;
SELECT JSON_VALID(@v1),
       (SELECT COUNT(*) = 3 FROM t1) AS t1_no_change,
       (SELECT COUNT(*) = 3 FROM t2) AS t2_no_change;

SET @v1 = NULL;

--echo
--echo # EXPLAIN multitable DELETE.
EXPLAIN FORMAT=JSON INTO @v1 DELETE t1, t2 FROM t1, t2;
SELECT JSON_VALID(@v1),
       (SELECT COUNT(*) = 3 FROM t1) AS t1_no_change,
       (SELECT COUNT(*) = 3 FROM t2) AS t2_no_change;

--echo
--echo # EXPLAIN ANALYZE INTO

# No effect on hypergraph
SET explain_json_format_version=2;

--echo
--echo # EXPLAIN ANALYZE SELECT.
EXPLAIN ANALYZE FORMAT=JSON INTO @v1 SELECT * FROM t1 JOIN t2 ON i1 = i3 WHERE i2 = 2;
--replace_regex $elide_json_costs_and_time
SELECT @v1, JSON_VALID(@v1);

--echo
--echo # User variable is overwritten on successful execution.
SET @v2 = @v1;
EXPLAIN ANALYZE FORMAT=JSON INTO @v1 SELECT * FROM t1;
SELECT JSON_VALID(@v1), @v1 != @v2, @v1 NOT LIKE CONCAT(@v2, '%');

SET @v1 = NULL;

--echo
--echo # EXPLAIN ANALYZE single table INSERT VALUES.
EXPLAIN ANALYZE FORMAT=JSON INTO @v1 INSERT INTO t1 VALUES (4,5);
SELECT JSON_VALID(@v1), COUNT(*) = 3 FROM t1;

SET @v1 = NULL;

--echo
--echo # EXPLAIN ANALYZE single table INSERT SELECT.
EXPLAIN ANALYZE FORMAT=JSON INTO @v1 INSERT INTO t1 SELECT * FROM t2;
SELECT JSON_VALID(@v1), COUNT(*) = 3 FROM t1;

SET @v1 = NULL;

--echo
--echo # EXPLAIN ANALYZE single table UPDATE.
EXPLAIN ANALYZE FORMAT=JSON INTO @v1 UPDATE t1 SET i2 = i2 + 1 WHERE i1 = 1;
SELECT JSON_VALID(@v1), i2 = 2 FROM t1 WHERE i1 = 1;

SET @v1 = NULL;

--echo
--echo # EXPLAIN ANALYZE single table REPLACE.
EXPLAIN ANALYZE FORMAT=JSON INTO @v1 REPLACE t1 SELECT * FROM t2;
SELECT JSON_VALID(@v1), COUNT(*) = 3 FROM t1;

SET @v1 = NULL;

--echo
--echo # EXPLAIN ANALYZE single table DELETE.
EXPLAIN ANALYZE FORMAT=JSON INTO @v1 DELETE FROM t1;
SELECT JSON_VALID(@v1), COUNT(*) = 3 FROM t1;

SET @v1 = NULL;

--echo
--echo # EXPLAIN ANALYZE multitable UPDATE.
EXPLAIN ANALYZE FORMAT=JSON INTO @v1 UPDATE t1, t2 SET i1 = i1 - 1, i3 = i3 + 1;
SELECT JSON_VALID(@v1),
       (SELECT COUNT(*) = 3 FROM t1) AS t1_no_change,
       (SELECT COUNT(*) = 3 FROM t2) AS t2_no_change;

SET @v1 = NULL;

--echo
--echo # EXPLAIN ANALYZE multitable DELETE.
EXPLAIN ANALYZE FORMAT=JSON INTO @v1 DELETE t1, t2 FROM t1, t2;
SELECT JSON_VALID(@v1),
       (SELECT COUNT(*) = 3 FROM t1) AS t1_no_change,
       (SELECT COUNT(*) = 3 FROM t2) AS t2_no_change;

DROP TABLE t1, t2;
SET @v1=NULL, @v2=NULL;
