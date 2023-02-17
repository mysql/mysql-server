--echo #
--echo # WL#15588 Store EXPLAIN FORMAT=JSON SELECT output in user variable.
--echo #

SET @v1 = 'UNCHANGED';
SET @v2 = @v1;

--echo
--echo # Unsupported functionality.
--error ER_EXPLAIN_INTO_ANALYZE_NOT_SUPPORTED
EXPLAIN ANALYZE FORMAT=JSON INTO @v1 SELECT 1;
--error ER_EXPLAIN_INTO_IMPLICIT_FORMAT_NOT_SUPPORTED
EXPLAIN INTO @v1 SELECT 1;
--error ER_EXPLAIN_INTO_FORMAT_NOT_SUPPORTED
EXPLAIN FORMAT=TRADITIONAL INTO @v1 SELECT 1;
--error ER_EXPLAIN_INTO_FORMAT_NOT_SUPPORTED
EXPLAIN FORMAT=TREE INTO @v1 SELECT 1;

--echo # Check that the variable was not changed on error.
SELECT @v1, @v1 = @v2;

--echo
CREATE TABLE t1 (i1 int, i2 int);
CREATE TABLE t2 (i3 int, i4 int);

INSERT INTO t1 VALUES (1,2), (2,3), (3,4);
INSERT INTO t2 SELECT i2, i1 FROM t1;
ANALYZE TABLE t1, t2;

--echo
--echo # Simple SELECT.
EXPLAIN FORMAT=JSON INTO @v1 SELECT * FROM t1 JOIN t2 ON i1 = i3 WHERE i2 = 2;
SELECT @v1, JSON_VALID(@v1) from dual;

--echo
--echo # User variable is overwritten on successful execution.
SET @v2 = @v1;
EXPLAIN FORMAT=JSON INTO @v1 SELECT * FROM t1;
SELECT JSON_VALID(@v1), @v1 != @v2, @v1 NOT LIKE CONCAT(@v2, '%');

DROP TABLE t1, t2;
SET @v1=NULL, @v2=NULL;
