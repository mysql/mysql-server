CREATE USER test@localhost;
CREATE USER test_2@localhost;
GRANT CREATE,INSERT,DELETE,DROP,SELECT ON test.* TO test@localhost;
GRANT CREATE,INSERT,DELETE,DROP,SELECT ON test.* TO test_2@localhost;
INSTALL COMPONENT "file://component_udf_reg_avg_func";
SELECT component_urn from mysql.component;
component_urn
file://component_udf_reg_avg_func
connect c1,localhost,test,,;
SELECT * FROM performance_schema.user_defined_functions
WHERE (udf_name LIKE 'myfunc%') or (udf_name LIKE 'avgcost%')
ORDER BY udf_name;
ERROR 42000: SELECT command denied to user 'test'@'localhost' for table 'user_defined_functions'
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
SELECT component_urn from mysql.component;
ERROR 42000: SELECT command denied to user 'test'@'localhost' for table 'component'
UNINSTALL COMPONENT "file://component_udf_reg_avg_func";
ERROR 42000: DELETE command denied to user 'test'@'localhost' for table 'component'
connection default;
GRANT CREATE,INSERT,DELETE,DROP,SELECT ON mysql.component TO test@localhost;
connection c1;
SELECT component_urn from mysql.component;
component_urn
file://component_udf_reg_avg_func
UNINSTALL COMPONENT "file://component_udf_reg_avg_func";
connection default;
disconnect c1;
INSTALL COMPONENT "file://component_udf_reg_int_func";
connect c2,localhost,test_2,,;
SELECT myfunc_int(23);
connect c1,localhost,test,,;
SELECT myfunc_int(23);
connection default;
SELECT udf_name,udf_return_type,udf_type,udf_library,udf_usage_count>0
FROM performance_schema.user_defined_functions
WHERE (udf_name LIKE 'myfunc%') or (udf_name LIKE 'avgcost%')
ORDER BY udf_name;
udf_name	udf_return_type	udf_type	udf_library	udf_usage_count>0
myfunc_int	integer	function	NULL	1
connection c1;
myfunc_int(23)
23
connection c2;
myfunc_int(23)
23
connection default;
UNINSTALL COMPONENT "file://component_udf_reg_int_func";
disconnect c1;
disconnect c2;
DROP USER test@localhost;
DROP USER test_2@localhost;
