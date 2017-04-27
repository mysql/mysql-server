--echo # Simple load test
INSTALL COMPONENT "file://component_test_string_service_long";
UNINSTALL COMPONENT "file://component_test_string_service_long";

# Write the test results in "test_string_service_long.log" into the result file of this test
--echo ########## test_string_service_long.log: 
let $MYSQLD_DATADIR= `select @@datadir`;
cat_file $MYSQLD_DATADIR/test_string_service_long.log;
remove_file $MYSQLD_DATADIR/test_string_service_long.log;



