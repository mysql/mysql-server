--echo #
--echo # WL#14722 Support IF NOT EXISTS clause
--echo # in CREATE PROCEDURE/FUNCTION/TRIGGER
--echo #

--echo
--echo # 1. Stored Procedures (SPs)
--echo
--echo # 1.1. Must execute successfully since the SP does not exist
CREATE PROCEDURE IF NOT EXISTS sp1() BEGIN END;
--echo # 1.2. Must report a warning that the SP already exists
CREATE PROCEDURE IF NOT EXISTS sp1() BEGIN END;
--echo # 1.3. Must fail with error that the SP already exists
--error ER_SP_ALREADY_EXISTS
CREATE PROCEDURE sp1() BEGIN END;
DROP PROCEDURE sp1;

--echo
--echo # 2. Stored Functions (SFs)
--echo
--echo # 2.1. Must execute successfully since the SF does not exist
CREATE FUNCTION IF NOT EXISTS sf1() RETURNS INT DETERMINISTIC return 0;
--echo # 2.2. Must report a warning since the SF already exists
CREATE FUNCTION IF NOT EXISTS sf1() RETURNS INT DETERMINISTIC return 0;
--echo # 2.3. Must fail with error since the SF already exists
--error ER_SP_ALREADY_EXISTS
CREATE FUNCTION sf1() RETURNS INT DETERMINISTIC return 0;
DROP FUNCTION sf1;
--echo # 2.4. Must report a warning since it has the same name as a native function
CREATE FUNCTION IF NOT EXISTS abs() RETURNS INT DETERMINISTIC return 0;
DROP FUNCTION abs;
--echo # 2.5. Must report a warning that UDF will override the SF
--replace_result $UDF_EXAMPLE_LIB UDF_EXAMPLE_LIB
--eval CREATE FUNCTION metaphon RETURNS STRING SONAME "$UDF_EXAMPLE_LIB"
CREATE FUNCTION IF NOT EXISTS metaphon() RETURNS INT DETERMINISTIC return 0;
DROP FUNCTION metaphon;
DROP FUNCTION test.metaphon;

--echo
--echo # 3. Loadable Functions / User Defined Functions (UDFs)
--echo
--echo # 3.1. Must execute successfully since the UDF does not exist
--replace_result $UDF_EXAMPLE_LIB UDF_EXAMPLE_LIB
--eval CREATE FUNCTION IF NOT EXISTS metaphon RETURNS STRING SONAME "$UDF_EXAMPLE_LIB"
--echo # 3.2. Must report a warning since the UDF already exists
--replace_result $UDF_EXAMPLE_LIB UDF_EXAMPLE_LIB
--eval CREATE FUNCTION IF NOT EXISTS metaphon RETURNS STRING SONAME "$UDF_EXAMPLE_LIB"
--echo # 3.3. Must fail with error since the UDF already exists
--replace_result $UDF_EXAMPLE_LIB UDF_EXAMPLE_LIB
--error ER_UDF_EXISTS
--eval CREATE FUNCTION metaphon RETURNS STRING SONAME "$UDF_EXAMPLE_LIB"
DROP FUNCTION metaphon;
--echo # 3.4. Must fail with an error since native function already exists
--replace_result $UDF_EXAMPLE_LIB UDF_EXAMPLE_LIB
--error ER_IF_NOT_EXISTS_UNSUPPORTED_UDF_NATIVE_FCT_NAME_COLLISION
--eval CREATE FUNCTION IF NOT EXISTS sum RETURNS INT SONAME "$UDF_EXAMPLE_LIB"
--echo # 3.5. Will succeed without warning, but UDF will override the SF
CREATE FUNCTION IF NOT EXISTS metaphon() RETURNS INT DETERMINISTIC return 0;
--replace_result $UDF_EXAMPLE_LIB UDF_EXAMPLE_LIB
--eval CREATE FUNCTION metaphon RETURNS STRING SONAME "$UDF_EXAMPLE_LIB"
DROP FUNCTION metaphon;
DROP FUNCTION test.metaphon;

--echo
--echo # 4. Triggers
--echo
CREATE TABLE t1 (a INT);
--echo # 4.1. Must execute successfully since the trigger does not exist
CREATE TRIGGER IF NOT EXISTS trg1 BEFORE INSERT ON t1 FOR EACH ROW BEGIN END;
--echo # 4.2. Must report a warning since the trigger already exists on the same table
CREATE TRIGGER IF NOT EXISTS trg1 BEFORE INSERT ON t1 FOR EACH ROW BEGIN END;
--echo # 4.3. Must fail with error since the trigger already exists on a different table
CREATE TABLE t2 (a INT);
--error ER_IF_NOT_EXISTS_UNSUPPORTED_TRG_EXISTS_ON_DIFFERENT_TABLE
CREATE TRIGGER IF NOT EXISTS trg1 BEFORE INSERT ON t2 FOR EACH ROW BEGIN END;
DROP TABLE t2;
--echo # 4.4. Must fail with error since the trigger already exists
--error ER_TRG_ALREADY_EXISTS
CREATE TRIGGER trg1 BEFORE INSERT ON t1 FOR EACH ROW BEGIN END;
DROP TABLE t1;

--echo
--echo #
--echo # WL#14774 Support IF NOT EXISTS clause
--echo # in CREATE VIEW
--echo #

--echo
--echo # 5. Views
--echo
--echo # 5.1. Must execute successfully since the view does not exist
CREATE TABLE t1 (a INT);
CREATE VIEW IF NOT EXISTS v1 (v1_a) AS SELECT a FROM t1;
--echo # 5.2. Must report a warning that the view already exists
CREATE VIEW IF NOT EXISTS v1 (v1_a) AS SELECT a FROM t1;
--echo # 5.3. Must fail with error that the view already exists
--error ER_TABLE_EXISTS_ERROR
CREATE VIEW v1 (v1_a) AS SELECT a FROM t1;
--echo # 5.4. Must report a warning that the table already exists
CREATE TABLE t2 (b INT);
CREATE VIEW IF NOT EXISTS t1 (t2_b) AS SELECT b FROM t2;
--echo # 5.5. Must fail with an error if both OR REPLACE and IF NOT EXISTS
--echo # clauses are present.
--error ER_PARSE_ERROR
CREATE OR REPLACE VIEW IF NOT EXISTS v1 (t2_b) AS SELECT b FROM t2;
--echo # 5.6. Must fail with an error when an IF NOT EXISTS clause is
--echo # present in an ALTER VIEW statement
--error ER_PARSE_ERROR
ALTER VIEW IF NOT EXISTS v1 (t2_b) AS SELECT b FROM t2;
--echo # 5.7. Must fail with an error to create a view when a table of
--echo # the same name already exists.
--error ER_TABLE_EXISTS_ERROR
CREATE VIEW t1 AS SELECT b from t2;
--echo # 5.8. Must fail with an error to create a table when a view of 
--echo # the same name already exists.
--error ER_TABLE_EXISTS_ERROR
CREATE TABLE v1 (c INT);
--echo # 5.9. Must fail with an error when creating a view with the same
--echo # name as an existing view and an incorrect view definition.
--error ER_NO_SUCH_TABLE
CREATE VIEW v1 AS SELECT * FROM invalid_table_name;
--error ER_BAD_FIELD_ERROR
CREATE VIEW v1 AS SELECT invalid_column_name FROM t1;

DROP VIEW v1;
DROP TABLE t1;
DROP TABLE t2;
