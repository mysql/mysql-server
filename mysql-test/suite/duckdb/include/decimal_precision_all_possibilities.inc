--echo #
--echo #1. prepare
--echo #
SELECT @@duckdb_dml_in_batch;

--echo #
--echo #2. duckdb_use_double_for_decimal = on (default)
--echo #
SET GLOBAL duckdb_use_double_for_decimal = on;


--echo #2.1 low precision(<=38)  with low precision value
CREATE TABLE t1(id int primary key, c1 decimal(38, 5)) engine=duckdb;
--echo # should be decimal(38, 5)
CALL dbms_duckdb.query("SELECT column_name, column_index, data_type, numeric_precision FROM duckdb_columns() WHERE schema_name = 'test' AND table_name = 't1' and column_name = 'c1'");


INSERT INTO t1 values (1, 123456789012345678901234567890123.12345);
SELECT * FROM t1 WHERE id = 1;

INSERT INTO t1 values (101, 0);
SELECT * FROM t1 WHERE id = 101;

INSERT INTO t1 values (102, 0.00000);
SELECT * FROM t1 WHERE id = 102;

--echo #2.2 low precision(<=38)  with high precision value(>38) 

--echo #ok, The precision after point can be lost
INSERT INTO t1 values (2, 123456789012345678901234567890123.123456);
SELECT * FROM t1 WHERE id = 2;

--echo #error, The precision before point can not be lost
--error 1264
INSERT INTO t1 values (3, 1234567890123456789012345678901234.123456);
SELECT * FROM t1 WHERE id = 3;
DROP TABLE t1;

--echo #2.3 high precision column(>38) with low precision value(<38)
CREATE TABLE t1(id int primary key, c1 decimal(40, 5)) engine=duckdb;
--echo # should be double
CALL dbms_duckdb.query("SELECT column_name, column_index, data_type, numeric_precision FROM duckdb_columns() WHERE schema_name = 'test' AND table_name = 't1' and column_name = 'c1'");

INSERT INTO t1 values (4, 123456789012345678901234567890.12345);
SELECT * FROM t1 WHERE id = 4;

--echo #2.4 high precision column(>38) with low precision value(<38)
INSERT INTO t1 values (5, 12345678901234567890123456789012345.12345678);
SELECT * FROM t1 WHERE id = 5;

--error 1264
INSERT INTO t1 values (6, 12345678901234567890123456789012345678.12345);
SELECT * FROM t1 WHERE id = 6;
CALL dbms_duckdb.query("INSERT INTO t1 values (5, 12345678901234567890123456789012345678.12345)");
SELECT * FROM t1 WHERE id = 6;
DROP TABLE t1;

--echo #
--echo #3. duckdb_use_double_for_decimal = false
--echo #
SET GLOBAL duckdb_use_double_for_decimal = off;

--echo #3.1 low precision(<=38)  with low precision value
CREATE TABLE t1(id int primary key, c1 decimal(38, 5)) engine=duckdb; 
--echo # should be decimal(38, 5)
CALL dbms_duckdb.query("SELECT column_name, column_index, data_type, numeric_precision FROM duckdb_columns() WHERE schema_name = 'test' AND table_name = 't1' and column_name = 'c1'");

INSERT INTO t1 values (7, 123456789012345678901234567890123.12345);
SELECT * FROM t1 WHERE id = 7;

INSERT INTO t1 values (701, 0);
SELECT * FROM t1 WHERE id = 701;

INSERT INTO t1 values (702, 0.00000);
SELECT * FROM t1 WHERE id = 702;

--echo #3.2 low precision(<=38)  with high precision value(>38) 

--echo #ok, The precision after point can be lost
INSERT INTO t1 values (8, 123456789012345678901234567890123.123456);
SELECT * FROM t1 WHERE id = 8;

--echo #error, The precision before point can not be lost
--error 1264
INSERT INTO t1 values (9, 1234567890123456789012345678901234.123456);
SELECT * FROM t1 WHERE id = 9;
DROP TABLE t1;

--echo #3.3 high precision column(>38) with low precision value(<38)
--echo #duckdb use decimal(38, dec) for both high and low precision(<=38) 
CREATE TABLE t1(id int primary key, c1 decimal(40, 5)) engine=duckdb;
--echo # should be decimal(38, 5)
CALL dbms_duckdb.query("SELECT column_name, column_index, data_type, numeric_precision FROM duckdb_columns() WHERE schema_name = 'test' AND table_name = 't1' and column_name = 'c1'");

INSERT INTO t1 values (10, 123456789012345678901234567890123.12345);
SELECT * FROM t1 WHERE id = 10;

--echo #3.4 high precision column(>38) with high precision value(>38)
--echo #duckdb use decimal(38, dec)

--error ER_DUCKDB_QUERY_ERROR, ER_DUCKDB_APPENDER_ERROR
INSERT INTO t1 values (11, 1234567890123456789012345678901234.12345);
SELECT * FROM t1 WHERE id = 11;

INSERT INTO t1 values (12, 123456789012345678901234567890123.12345678);
SELECT * FROM t1 WHERE id = 12;
DROP TABLE t1;
set global duckdb_use_double_for_decimal = default;

