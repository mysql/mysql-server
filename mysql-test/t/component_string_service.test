--echo # Simple load test
INSTALL COMPONENT "file://component_test_string_service";
UNINSTALL COMPONENT "file://component_test_string_service";

# Write the test results in "test_string_service.log" into the result file of this test
--echo ########## test_string_service.log: 
let $MYSQLD_DATADIR= `select @@datadir`;
cat_file $MYSQLD_DATADIR/test_string_service.log;
remove_file $MYSQLD_DATADIR/test_string_service.log;



