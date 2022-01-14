#
# WL#14722 Support IF NOT EXISTS clause
# in CREATE PROCEDURE/FUNCTION/TRIGGER
#

# 1. Stored Procedures (SPs)

# 1.1. Must execute successfully since the SP does not exist
CREATE PROCEDURE IF NOT EXISTS sp1() BEGIN END;
# 1.2. Must report a warning that the SP already exists
CREATE PROCEDURE IF NOT EXISTS sp1() BEGIN END;
Warnings:
Note	1304	PROCEDURE sp1 already exists
# 1.3. Must fail with error that the SP already exists
CREATE PROCEDURE sp1() BEGIN END;
ERROR 42000: PROCEDURE sp1 already exists
DROP PROCEDURE sp1;

# 2. Stored Functions (SFs)

# 2.1. Must execute successfully since the SF does not exist
CREATE FUNCTION IF NOT EXISTS sf1() RETURNS INT DETERMINISTIC return 0;
# 2.2. Must report a warning since the SF already exists
CREATE FUNCTION IF NOT EXISTS sf1() RETURNS INT DETERMINISTIC return 0;
Warnings:
Note	1304	FUNCTION sf1 already exists
# 2.3. Must fail with error since the SF already exists
CREATE FUNCTION sf1() RETURNS INT DETERMINISTIC return 0;
ERROR 42000: FUNCTION sf1 already exists
DROP FUNCTION sf1;
# 2.4. Must report a warning since it has the same name as a native function
CREATE FUNCTION IF NOT EXISTS abs() RETURNS INT DETERMINISTIC return 0;
Warnings:
Note	1585	This function 'abs' has the same name as a native function
DROP FUNCTION abs;
# 2.5. Must report a warning that UDF will override the SF
CREATE FUNCTION metaphon RETURNS STRING SONAME "UDF_EXAMPLE_LIB";
CREATE FUNCTION IF NOT EXISTS metaphon() RETURNS INT DETERMINISTIC return 0;
Warnings:
Note	4084	This function 'metaphon' has the same name as a loadable function (UDF). To invoke the stored function, it is necessary to qualify it with the schema name.
DROP FUNCTION metaphon;
DROP FUNCTION test.metaphon;

# 3. Loadable Functions / User Defined Functions (UDFs)

# 3.1. Must execute successfully since the UDF does not exist
CREATE FUNCTION IF NOT EXISTS metaphon RETURNS STRING SONAME "UDF_EXAMPLE_LIB";
# 3.2. Must report a warning since the UDF already exists
CREATE FUNCTION IF NOT EXISTS metaphon RETURNS STRING SONAME "UDF_EXAMPLE_LIB";
Warnings:
Note	1125	Function 'metaphon' already exists
# 3.3. Must fail with error since the UDF already exists
CREATE FUNCTION metaphon RETURNS STRING SONAME "UDF_EXAMPLE_LIB";
ERROR HY000: Function 'metaphon' already exists
DROP FUNCTION metaphon;
# 3.4. Must fail with an error since native function already exists
CREATE FUNCTION IF NOT EXISTS sum RETURNS INT SONAME "UDF_EXAMPLE_LIB";
ERROR HY000: This function 'sum' has the same name as a native function. The 'IF NOT EXISTS' clause is not supported while creating a loadable function with the same name as a native function.
# 3.5. Will succeed without warning, but UDF will override the SF
CREATE FUNCTION IF NOT EXISTS metaphon() RETURNS INT DETERMINISTIC return 0;
CREATE FUNCTION metaphon RETURNS STRING SONAME "UDF_EXAMPLE_LIB";
DROP FUNCTION metaphon;
DROP FUNCTION test.metaphon;

# 4. Triggers

CREATE TABLE t1 (a INT);
# 4.1. Must execute successfully since the trigger does not exist
CREATE TRIGGER IF NOT EXISTS trg1 BEFORE INSERT ON t1 FOR EACH ROW BEGIN END;
# 4.2. Must report a warning since the trigger already exists on the same table
CREATE TRIGGER IF NOT EXISTS trg1 BEFORE INSERT ON t1 FOR EACH ROW BEGIN END;
Warnings:
Note	4099	Trigger 'trg1' already exists on the table 'test'.'t1'.
# 4.3. Must fail with error since the trigger already exists on a different table
CREATE TABLE t2 (a INT);
CREATE TRIGGER IF NOT EXISTS trg1 BEFORE INSERT ON t2 FOR EACH ROW BEGIN END;
ERROR HY000: Trigger 'test'.'trg1' already exists on a different table. The 'IF NOT EXISTS' clause is only supported for triggers associated with the same table.
DROP TABLE t2;
# 4.4. Must fail with error since the trigger already exists
CREATE TRIGGER trg1 BEFORE INSERT ON t1 FOR EACH ROW BEGIN END;
ERROR HY000: Trigger already exists
DROP TABLE t1;
