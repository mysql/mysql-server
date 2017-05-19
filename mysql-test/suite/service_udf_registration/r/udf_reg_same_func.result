INSTALL COMPONENT "file://component_udf_reg_int_same_func";
select myfunc_int(5);
myfunc_int(5)
5
select myfunc_int(5,6,9);
myfunc_int(5,6,9)
20
SELECT * FROM performance_schema.user_defined_functions
WHERE udf_name IN('myfunc_int')
ORDER BY udf_name;
UDF_NAME	UDF_RETURN_TYPE	UDF_TYPE	UDF_LIBRARY	UDF_USAGE_COUNT
myfunc_int	integer	function		1
INSTALL COMPONENT "file://component_udf_reg_int_func";
ERROR HY000: Initialization method provided by component 'mysql:test_udf_registration' failed.
select myfunc_int(5);
myfunc_int(5)
5
select myfunc_int(5,6,9);
myfunc_int(5,6,9)
20
SELECT * FROM performance_schema.user_defined_functions
WHERE udf_name IN('myfunc_int')
ORDER BY udf_name;
UDF_NAME	UDF_RETURN_TYPE	UDF_TYPE	UDF_LIBRARY	UDF_USAGE_COUNT
myfunc_int	integer	function		1
UNINSTALL COMPONENT "file://component_udf_reg_int_same_func";
SELECT * FROM performance_schema.user_defined_functions
WHERE udf_name IN('myfunc_int')
ORDER BY udf_name;
UDF_NAME	UDF_RETURN_TYPE	UDF_TYPE	UDF_LIBRARY	UDF_USAGE_COUNT
select myfunc_int(5);
ERROR 42000: FUNCTION test.myfunc_int does not exist
UNINSTALL COMPONENT "file://component_udf_reg_int_func";
ERROR HY000: Component specified by URN 'file://component_udf_reg_int_func' to unload has not been loaded before.
