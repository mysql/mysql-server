drop table if exists t1;
CREATE FUNCTION myfunc_double RETURNS REAL SONAME "UDF_EXAMPLE_LIB";
CREATE FUNCTION myfunc_int RETURNS INTEGER SONAME "UDF_EXAMPLE_LIB";
CREATE AGGREGATE FUNCTION avgcost RETURNS REAL SONAME "UDF_EXAMPLE_LIB";
SELECT myfunc_double();
ERROR HY000: Can't initialize function 'myfunc_double'; myfunc_double must have at least one argument
SELECT myfunc_double(1);
myfunc_double(1)
49.00
SELECT myfunc_double(78654);
myfunc_double(78654)
54.00
SELECT myfunc_int();
myfunc_int()
0
SELECT myfunc_int(5);
myfunc_int(5)
5
SELECT myfunc_int('MySQL V8.0');
myfunc_int('MySQL V8.0')
10
CREATE TABLE t1(a INT, b INT);
INSERT INTO t1 values (1,1),(2,2);
SELECT myfunc_int(a AS attr_name) FROM t1;
myfunc_int(a AS attr_name)
1
2
DROP TABLE t1;
CREATE TABLE t1(sum int, price float(24));
INSERT INTO t1 VALUES(100, 50.00), (100, 100.00);
SELECT avgcost(sum, price) from t1;
avgcost(sum, price)
75.0000
DELETE from t1;
INSERT INTO t1 VALUES(100, 54.33), (200, 199.99);
SELECT avgcost(sum, price) from t1;
avgcost(sum, price)
151.4367
DROP TABLE t1;
DROP FUNCTION myfunc_double;
DROP FUNCTION myfunc_int;
DROP FUNCTION avgcost;
SELECT myfunc_double(1);
ERROR 42000: FUNCTION test.myfunc_double does not exist
SELECT myfunc_int(1);
ERROR 42000: FUNCTION test.myfunc_int does not exist
CREATE FUNCTION myfunc_double RETURNS REAL SONAME "UDF_EXAMPLE_LIB";
CREATE FUNCTION myfunc_int RETURNS INTEGER SONAME "UDF_EXAMPLE_LIB";
CREATE AGGREGATE FUNCTION avgcost RETURNS REAL SONAME "UDF_EXAMPLE_LIB";
SELECT myfunc_double(78654);
myfunc_double(78654)
54.00
SELECT myfunc_int(5);
myfunc_int(5)
5
DROP FUNCTION myfunc_int;
SELECT myfunc_double(78654);
myfunc_double(78654)
54.00
SELECT myfunc_int(5);
ERROR 42000: FUNCTION test.myfunc_int does not exist
DROP FUNCTION myfunc_double;
DROP FUNCTION avgcost;
