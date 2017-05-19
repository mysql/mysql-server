INSTALL COMPONENT "file://component_udf_reg_only_3_func";
select myfunc_double();
ERROR HY000: Can't initialize function 'myfunc_double'; myfunc_double must have at least one argument
select myfunc_double(1);
myfunc_double(1)
49.00
select myfunc_double(78654);
myfunc_double(78654)
54.00
DROP FUNCTION myfunc_double;
ERROR HY000: DROP FUNCTION can't drop a dynamically registered user defined function
CREATE FUNCTION myfunc_double RETURNS REAL SONAME "UDF_EXAMPLE_LIB";
ERROR HY000: Function 'myfunc_double' already exists
select myfunc_int();
myfunc_int()
0
select myfunc_int(5);
myfunc_int(5)
5
select myfunc_int('MySQL V8.0');
myfunc_int('MySQL V8.0')
10
DROP FUNCTION myfunc_int;
ERROR HY000: DROP FUNCTION can't drop a dynamically registered user defined function
CREATE FUNCTION myfunc_int RETURNS INTEGER SONAME "UDF_EXAMPLE_LIB";
ERROR HY000: Function 'myfunc_int' already exists
CREATE TABLE t1(a INT, b INT);
INSERT INTO t1 values (1,1),(2,2);
SELECT myfunc_int(a AS attr_name) FROM t1;
myfunc_int(a AS attr_name)
1
2
drop table t1;
create table t1(sum int, price float(24));
insert into t1 values(100, 50.00), (100, 100.00);
select avgcost(sum, price) from t1;
avgcost(sum, price)
75.0000
delete from t1;
insert into t1 values(100, 54.33), (200, 199.99);
select avgcost(sum, price) from t1;
avgcost(sum, price)
151.4367
drop table t1;
DROP FUNCTION avgcost;
ERROR HY000: DROP FUNCTION can't drop a dynamically registered user defined function
# remove the plugin
INSTALL COMPONENT "file://component_udf_unreg_3_func";
select myfunc_double(1);
ERROR 42000: FUNCTION test.myfunc_double does not exist
select myfunc_int(5);
ERROR 42000: FUNCTION test.myfunc_int does not exist
CREATE FUNCTION myfunc_double RETURNS REAL SONAME "UDF_EXAMPLE_LIB";
select myfunc_double(1);
myfunc_double(1)
49.00
DROP FUNCTION myfunc_double;
UNINSTALL COMPONENT "file://component_udf_reg_only_3_func";
UNINSTALL COMPONENT "file://component_udf_unreg_3_func";
