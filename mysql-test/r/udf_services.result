#
# Bug #20085672: CRYPTIC ERROR WHEN FAILING TO UNLOAD A DYNAMIC LIBRARY
#
# Install the plugin
INSTALL PLUGIN test_udf_services SONAME 'TESTUDFSERVICES';
# Define the function
CREATE FUNCTION test_udf_services_udf RETURNS INT
SONAME "TESTUDFSERVICES";
# Uninstall the plugin
UNINSTALL PLUGIN test_udf_services;
# Install the plugin again. Should not fail
INSTALL PLUGIN test_udf_services SONAME 'TESTUDFSERVICES';
# Cleanup
DROP FUNCTION test_udf_services_udf;
UNINSTALL PLUGIN test_udf_services;
