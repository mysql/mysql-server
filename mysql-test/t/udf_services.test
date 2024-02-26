
--echo #
--echo # Bug #20085672: CRYPTIC ERROR WHEN FAILING TO UNLOAD A DYNAMIC LIBRARY
--echo #

--echo # Install the plugin
--replace_result $TESTUDFSERVICES TESTUDFSERVICES
eval INSTALL PLUGIN test_udf_services SONAME '$TESTUDFSERVICES';

--echo # Define the function
--replace_result $TESTUDFSERVICES TESTUDFSERVICES
eval CREATE FUNCTION test_udf_services_udf RETURNS INT
  SONAME "$TESTUDFSERVICES";

--echo # Uninstall the plugin
UNINSTALL PLUGIN test_udf_services;

--echo # Install the plugin again. Should not fail
--replace_result $TESTUDFSERVICES TESTUDFSERVICES
eval INSTALL PLUGIN test_udf_services SONAME '$TESTUDFSERVICES';

--echo # Cleanup
DROP FUNCTION test_udf_services_udf;
UNINSTALL PLUGIN test_udf_services;
