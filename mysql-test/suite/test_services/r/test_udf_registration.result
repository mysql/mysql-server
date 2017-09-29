INSTALL PLUGIN test_udf_registration SONAME 'test_udf_services.so';
# Must work
SELECT test_udf_registration_udf();
test_udf_registration_udf()
0
UNINSTALL PLUGIN test_udf_registration;
# Must fail: no UDF
SELECT test_udf_registration_udf();
ERROR 42000: FUNCTION test.test_udf_registration_udf does not exist
INSTALL PLUGIN test_udf_registration SONAME 'test_udf_services.so';
# Restart the server
# restart
# Must work after a restart
SELECT test_udf_registration_udf();
test_udf_registration_udf()
0
# Cleanup
UNINSTALL PLUGIN test_udf_registration;
