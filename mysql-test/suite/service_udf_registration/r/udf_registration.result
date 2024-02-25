# Simple load test
INSTALL COMPONENT "file://component_test_udf_registration";
SELECT * FROM performance_schema.user_defined_functions
WHERE UDF_NAME IN('dynamic_udf', 'dynamic_agg')
ORDER BY UDF_NAME;
UDF_NAME	UDF_RETURN_TYPE	UDF_TYPE	UDF_LIBRARY	UDF_USAGE_COUNT
dynamic_agg	integer	aggregate	NULL	1
dynamic_udf	integer	function	NULL	1
# Must return 12
SELECT dynamic_udf();
dynamic_udf()
42
# aggregate function test
CREATE TABLE t1(a INT, b INT);
INSERT INTO t1 VALUES (1,1),(2,1),(3,2),(4,4);
# aggregate all rows: expect 42
SELECT dynamic_agg(a) FROM t1;
dynamic_agg(a)
42
# aggregate with group by: expect 42
SELECT dynamic_agg(a) FROM t1 GROUP BY b;
dynamic_agg(a)
42
42
42
DROP TABLE t1;
# DROP should fail
DROP FUNCTION dynamic_udf;
ERROR HY000: DROP FUNCTION can't drop a dynamically registered user defined function
# Activating the sync point at udf reference count decrease
SET DEBUG_SYNC='udf_handler_destroy_sync SIGNAL pre_cleanup WAIT_FOR continue TIMEOUT 20';
# now emit a call to lock the UDF
SELECT dynamic_udf();
# switch to c1;
SET DEBUG_SYNC='now WAIT_FOR pre_cleanup';
# use counts should be 2 for dynamic_udf and 1 for dynamic_agg
SELECT UDF_NAME, UDF_USAGE_COUNT FROM performance_schema.user_defined_functions
WHERE UDF_NAME IN('dynamic_udf', 'dynamic_agg')
ORDER BY UDF_NAME;
UDF_NAME	UDF_USAGE_COUNT
dynamic_agg	1
dynamic_udf	2
# Uninstall should fail: dynamic_udf stil locked
UNINSTALL COMPONENT "file://component_test_udf_registration";
ERROR HY000: De-initialization method provided by component 'mysql:test_udf_registration' failed.
# release the reference count decrease
SET DEBUG_SYNC='now SIGNAL continue';
# switch back to default connection
# reaping the result from SELECT
dynamic_udf()
42
# should pass: because this udf is in use when the above Uninstall
# component is executed and the component deinit function failed as
# this udf is in use.
SELECT dynamic_udf();
dynamic_udf()
42
# Should fail: this one is unregistered, because this udf is not in use
# when the above Uninstall component is executed.
SELECT dynamic_agg(a);
ERROR 42000: FUNCTION test.dynamic_agg does not exist
# dynamic_udf should present and dynamic_agg should not.
SELECT * FROM performance_schema.user_defined_functions
WHERE UDF_NAME IN('dynamic_udf', 'dynamic_agg')
ORDER BY UDF_NAME;
UDF_NAME	UDF_RETURN_TYPE	UDF_TYPE	UDF_LIBRARY	UDF_USAGE_COUNT
dynamic_udf	integer	function	NULL	1
# remove the plugin
UNINSTALL COMPONENT "file://component_test_udf_registration";
# should fail: no UDF
SELECT dynamic_udf();
ERROR 42000: FUNCTION test.dynamic_udf does not exist
