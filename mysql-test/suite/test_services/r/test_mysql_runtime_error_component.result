# Test mysql_runtime_error service, similar signature
INSTALL COMPONENT "file://component_test_mysql_runtime_error";
# Test mysql_runtime_error utility function
SELECT test_mysql_runtime_error(0);
ERROR HY000: Component specified by URN 'This is to test the mysql_runtime_error service using utility function' to unload has not been loaded before.
# Test mysql_runtime_error service, similar signature
# as my_error function
SELECT test_mysql_runtime_error(1);
ERROR HY000: Component specified by URN 'This is to test the mysql_runtime_error service' to unload has not been loaded before.
# Test mysql_runtime_error service, similar signature
# as my_error function
SELECT test_mysql_runtime_error(3);
ERROR HY000: Unregistration of service implementation 'This is to test the mysql_runtime_error service' provided by component ' using utility function' failed during unloading of the component.
# Test mysql_runtime_error service, similar signature
# as my_error function
SELECT test_mysql_runtime_error(4);
ERROR HY000: Invalid thread priority value 123 for Test resource group Test group. Allowed range is [0, 99].
# Test mysql_runtime_error service, similar signature
# as my_error function
SELECT test_mysql_runtime_error(5);
ERROR HY000: Timeout exceeded in regular expression match.
# Test mysql_runtime_error service, similar signature
# as my_error function
SELECT test_mysql_runtime_error(6);
ERROR 42000: Specified key was too long; max key length is 1024 bytes
# Test mysql_runtime_error service, similar signature
# as my_error function
SELECT test_mysql_runtime_error(7);
ERROR HY000: Unregistration of service implementation 'This is to test the mysql_runtime_error service' provided by component ' using utility function' failed during unloading of the component.
# Test mysql_runtime_error service, similar signature
# as my_error function
SELECT test_mysql_runtime_error(8);
ERROR HY000: Invalid thread priority value 123 for Test resource group Test group. Allowed range is [0, 99].
# Test mysql_runtime_error service, similar signature
# as my_error function
SELECT test_mysql_runtime_error(9);
ERROR HY000: Timeout exceeded in regular expression match.
# Test mysql_runtime_error service, similar signature
# as my_error function
SELECT test_mysql_runtime_error(10);
ERROR 42000: Specified key was too long; max key length is 1024 bytes
UNINSTALL COMPONENT "file://component_test_mysql_runtime_error";
