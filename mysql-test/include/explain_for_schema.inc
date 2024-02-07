--source include/elide_costs.inc

--echo #
--echo # WL#15785 EXPLAIN FOR SCHEMA
--echo #

CREATE SCHEMA test_explain_for_schema;

# Not JSON columns due to Bug#34727172
CREATE TABLE explain_outputs (in_schema TEXT, for_schema TEXT);

USE test_explain_for_schema;
CREATE TABLE t1 (i1 INT, i2 INT);
CREATE TABLE t2 (i3 INT, i4 INT);
INSERT INTO t1 VALUES (1, 2), (3, 4), (5, 6);
INSERT INTO t2 SELECT i2, i1 FROM t1;

ANALYZE TABLE t1, t2;

# Use EXPLAIN INTO to limit the amount of output
--echo # A sample EXPLAIN for each type of explainable statement
EXPLAIN FORMAT=JSON INTO @in_schema_1 SELECT 1;
EXPLAIN FORMAT=JSON INTO @in_schema_2 SELECT * FROM t1, t2;
EXPLAIN FORMAT=JSON INTO @in_schema_3 SELECT * FROM t1 JOIN t2 ON i1 = i3 WHERE i2 = 2;
EXPLAIN FORMAT=JSON INTO @in_schema_4 INSERT INTO t1 VALUES (4,5);
EXPLAIN FORMAT=JSON INTO @in_schema_5 INSERT INTO t1 SELECT * FROM t2;
EXPLAIN FORMAT=JSON INTO @in_schema_6 UPDATE t1 SET i2 = i2 + 1 WHERE i1 = 1;
EXPLAIN FORMAT=JSON INTO @in_schema_7 REPLACE t1 SELECT * FROM t2;
EXPLAIN FORMAT=JSON INTO @in_schema_8 DELETE FROM t1;
EXPLAIN FORMAT=JSON INTO @in_schema_9 UPDATE t1, t2 SET i1 = i1 - 1, i3 = i3 + 1;
EXPLAIN FORMAT=JSON INTO @in_schema_10 DELETE t1, t2 FROM t1, t2 WHERE i3 = 4;

USE test;

EXPLAIN FORMAT=JSON INTO @for_schema_1 FOR SCHEMA test_explain_for_schema SELECT 1;
EXPLAIN FORMAT=JSON INTO @for_schema_2 FOR SCHEMA test_explain_for_schema SELECT * FROM t1, t2;
EXPLAIN FORMAT=JSON INTO @for_schema_3 FOR SCHEMA test_explain_for_schema SELECT * FROM t1 JOIN t2 ON i1 = i3 WHERE i2 = 2;
EXPLAIN FORMAT=JSON INTO @for_schema_4 FOR SCHEMA test_explain_for_schema INSERT INTO t1 VALUES (4,5);
EXPLAIN FORMAT=JSON INTO @for_schema_5 FOR SCHEMA test_explain_for_schema INSERT INTO t1 SELECT * FROM t2;
EXPLAIN FORMAT=JSON INTO @for_schema_6 FOR SCHEMA test_explain_for_schema UPDATE t1 SET i2 = i2 + 1 WHERE i1 = 1;
EXPLAIN FORMAT=JSON INTO @for_schema_7 FOR SCHEMA test_explain_for_schema REPLACE t1 SELECT * FROM t2;
EXPLAIN FORMAT=JSON INTO @for_schema_8 FOR SCHEMA test_explain_for_schema DELETE FROM t1;
EXPLAIN FORMAT=JSON INTO @for_schema_9 FOR SCHEMA test_explain_for_schema UPDATE t1, t2 SET i1 = i1 - 1, i3 = i3 + 1;
EXPLAIN FORMAT=JSON INTO @for_schema_10 FOR SCHEMA test_explain_for_schema DELETE t1, t2 FROM t1, t2 WHERE i3 = 4;

INSERT INTO explain_outputs (in_schema, for_schema) VALUES (@in_schema_1, @for_schema_1),
                                   (@in_schema_2, @for_schema_2),
                                   (@in_schema_3, @for_schema_3),
                                   (@in_schema_4, @for_schema_4),
                                   (@in_schema_5, @for_schema_5),
                                   (@in_schema_6, @for_schema_6),
                                   (@in_schema_7, @for_schema_7),
                                   (@in_schema_8, @for_schema_8),
                                   (@in_schema_9, @for_schema_9),
                                   (@in_schema_10, @for_schema_10);

SELECT JSON_VALID(in_schema), JSON_VALID(for_schema), in_schema = for_schema FROM explain_outputs;

--echo # Check every format is supported
# Traditional format is not supported for hypergraph optimizer
--error 0, ER_HYPERGRAPH_NOT_SUPPORTED_YET
EXPLAIN FOR SCHEMA test_explain_for_schema SELECT * FROM t1;
--error 0, ER_HYPERGRAPH_NOT_SUPPORTED_YET
EXPLAIN FORMAT=TRADITIONAL FOR SCHEMA test_explain_for_schema SELECT * FROM t1;
--replace_regex $elide_costs_and_rows
EXPLAIN FORMAT=TREE FOR SCHEMA test_explain_for_schema SELECT * FROM t1;
EXPLAIN FORMAT=JSON INTO @format_test FOR SCHEMA test_explain_for_schema SELECT * FROM t1;
SELECT JSON_VALID(@format_test);

--echo # Check other errors
--error ER_PARSE_ERROR
EXPLAIN FOR SCHEMA test_explain_for_schema FOR CONNECTION 1;
--error ER_PARSE_ERROR
EXPLAIN FORMAT=JSON FOR SCHEMA test_explain_for_schema INTO @dummy_var SELECT 1;
--error ER_PARSE_ERROR
EXPLAIN FOR SCHEMA test_explain_for_schema FORMAT=JSON INTO @dummy_var SELECT 1;
--error ER_BAD_DB_ERROR
EXPLAIN FOR SCHEMA non_existent_schema SELECT 1;
# Same error as regular EXPLAIN in schema
--error ER_NO_SUCH_TABLE
EXPLAIN FOR SCHEMA test_explain_for_schema SELECT * FROM non_existent_table;
--error ER_WRONG_DB_NAME
EXPLAIN FOR SCHEMA '' SELECT 1;
--error ER_PARSE_ERROR
EXPLAIN FOR SCHEMA SELECT 1;

SELECT @dummy_var IS NULL;

SET @explain_output = NULL;
SET @schema_before_create_proc = SCHEMA();

--echo # Check correct schema is restored after EXPLAIN FOR SCHEMA in nested procedure calls and prepared statement
DELIMITER |;
CREATE PROCEDURE prepare_and_execute_cmd (IN command VARCHAR(256))
BEGIN
    SET @prepare_statement_cmd = command;
    PREPARE stmt FROM @prepare_statement_cmd;
    EXECUTE stmt;
END|

CREATE PROCEDURE make_and_execute_explain_stmt ()
BEGIN
    SET @command = CONCAT('EXPLAIN FORMAT=JSON INTO @explain_output FOR SCHEMA ', @schema_name, ' ', @query_text);
    CALL prepare_and_execute_cmd(@command);
END|
DELIMITER ;|

SET @schema_after_create_proc = SCHEMA();

SET @schema_name = "test_explain_for_schema",
    @query_text  = "SELECT i1, i3, i4 FROM t1 JOIN t2 ON i1 = i3 WHERE i2 = 2";

CALL make_and_execute_explain_stmt;

SET @schema_after_call_proc = SCHEMA();

SELECT JSON_VALID(@explain_output), @schema_before_create_proc, @schema_after_create_proc,
       @schema_after_call_proc,
       @schema_before_create_proc = @schema_after_create_proc,
       @schema_after_create_proc = @schema_after_call_proc;

--echo # Check unqualified table names in SP uses schema from FOR SCHEMA and not from SP
DELIMITER |;
CREATE PROCEDURE p ()
BEGIN
    EXPLAIN FORMAT=TREE FOR SCHEMA test_explain_for_schema SELECT * FROM t1 JOIN t2 ON i1 = i3 WHERE i2 = 2;
END|
DELIMITER ;|
--replace_regex $elide_costs_and_rows
CALL p;

DROP PROCEDURE p;

--echo # Check that correct schema is used to qualify functions in an SP
DELIMITER |;
CREATE FUNCTION test_explain_for_schema.f () RETURNS INT DETERMINISTIC LANGUAGE SQL BEGIN RETURN 2; END|
CREATE PROCEDURE p () BEGIN EXPLAIN FORMAT=TREE FOR SCHEMA test_explain_for_schema SELECT f(); END|
DELIMITER ;|
--replace_regex $elide_costs_and_rows
CALL p;

--echo # Check that EXPLAIN FOR SCHEMA does not give access where access is not granted
CREATE USER access_check;
GRANT ALL ON test.* TO access_check;
change_user access_check;
USE test;
--error ER_DBACCESS_DENIED_ERROR
EXPLAIN FORMAT=TREE FOR SCHEMA test_explain_for_schema SELECT * FROM t1 JOIN t2 ON i1 = i3 WHERE i2 = 2;
change_user root;
USE test;
DROP USER access_check;

DROP PROCEDURE p;
DROP PROCEDURE make_and_execute_explain_stmt;
DROP PROCEDURE prepare_and_execute_cmd;
SET @in_schema_1 = NULL, @for_schema_1 = NULL,
    @in_schema_2 = NULL, @for_schema_2 = NULL,
    @in_schema_3 = NULL, @for_schema_3 = NULL,
    @in_schema_4 = NULL, @for_schema_4 = NULL,
    @in_schema_5 = NULL, @for_schema_5 = NULL,
    @in_schema_6 = NULL, @for_schema_6 = NULL,
    @in_schema_7 = NULL, @for_schema_7 = NULL,
    @in_schema_8 = NULL, @for_schema_8 = NULL,
    @in_schema_9 = NULL, @for_schema_9 = NULL,
    @in_schema_10 = NULL, @for_schema_10 = NULL,
    @schema_name = NULL, @query_text = NULL, @explain_output = NULL,
    @schema_before_create_proc = NULL, @schema_after_create_proc = NULL,
    @schema_after_call_proc = NULL;
DROP TABLE explain_outputs;
DROP SCHEMA test_explain_for_schema;
