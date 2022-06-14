# Simple load test
INSTALL COMPONENT "file://component_example_component1";
UNINSTALL COMPONENT "file://component_example_component1";
# Double load test
INSTALL COMPONENT "file://component_example_component1";
INSTALL COMPONENT "file://component_example_component1";
ERROR HY000: Cannot load component from specified URN: 'file://component_example_component1'.
UNINSTALL COMPONENT "file://component_example_component1";
# Load not existing
INSTALL COMPONENT "file://component_example_component4";
ERROR HY000: Can't open shared library
# Load with unsatisfied dependencies
INSTALL COMPONENT "file://component_example_component3";
ERROR HY000: Cannot satisfy dependency for service 'greetings' required by component 'mysql:example_component3'.
# Load with no schema
INSTALL COMPONENT "file:component_example_component3";
ERROR HY000: Cannot find schema in specified URN: 'file:component_example_component3'.
# Load with bad schema
INSTALL COMPONENT "bad_scheme://component_example_component3";
ERROR HY000: Cannot acquire scheme load service implementation for schema 'bad_scheme' in specified URN: 'bad_scheme://component_example_component3'.
# Load with path
INSTALL COMPONENT "file:///component_example_component3";
ERROR HY000: Cannot load component from specified URN: 'file:///component_example_component3'.
INSTALL COMPONENT "file://./component_example_component3";
ERROR HY000: Cannot load component from specified URN: 'file://./component_example_component3'.
INSTALL COMPONENT "file://../component_example_component3";
ERROR HY000: Cannot load component from specified URN: 'file://../component_example_component3'.
# Load with unsatisfied dependencies after unload
INSTALL COMPONENT "file://component_example_component1";
UNINSTALL COMPONENT "file://component_example_component1";
INSTALL COMPONENT "file://component_example_component3";
ERROR HY000: Cannot satisfy dependency for service 'greetings' required by component 'mysql:example_component3'.
# Unload not existing
UNINSTALL COMPONENT "file://component_example_component4";
ERROR HY000: Component specified by URN 'file://component_example_component4' to unload has not been loaded before.
# Unload not loaded
UNINSTALL COMPONENT "file://component_example_component1";
ERROR HY000: Component specified by URN 'file://component_example_component1' to unload has not been loaded before.
UNINSTALL COMPONENT "file://component_example_component1";
ERROR HY000: Component specified by URN 'file://component_example_component1' to unload has not been loaded before.
# Load/unload group of components without dependencies
INSTALL COMPONENT "file://component_example_component1", "file://component_example_component2";
UNINSTALL COMPONENT "file://component_example_component1", "file://component_example_component2";
INSTALL COMPONENT "file://component_example_component1", "file://component_example_component2";
UNINSTALL COMPONENT "file://component_example_component2", "file://component_example_component1";
INSTALL COMPONENT "file://component_example_component1", "file://component_example_component2";
UNINSTALL COMPONENT "file://component_example_component1";
UNINSTALL COMPONENT "file://component_example_component2";
INSTALL COMPONENT "file://component_example_component1", "file://component_example_component2";
UNINSTALL COMPONENT "file://component_example_component2";
UNINSTALL COMPONENT "file://component_example_component1";
INSTALL COMPONENT "file://component_example_component1";
INSTALL COMPONENT "file://component_example_component2";
UNINSTALL COMPONENT "file://component_example_component2", "file://component_example_component1";
INSTALL COMPONENT "file://component_example_component1";
INSTALL COMPONENT "file://component_example_component2";
UNINSTALL COMPONENT "file://component_example_component1", "file://component_example_component2";
# Load/unload group of components with dependencies
INSTALL COMPONENT "file://component_example_component1", "file://component_example_component3";
UNINSTALL COMPONENT "file://component_example_component1", "file://component_example_component3";
INSTALL COMPONENT "file://component_example_component3", "file://component_example_component1";
UNINSTALL COMPONENT "file://component_example_component1", "file://component_example_component3";
INSTALL COMPONENT "file://component_example_component3", "file://component_example_component1";
UNINSTALL COMPONENT "file://component_example_component3", "file://component_example_component1";
INSTALL COMPONENT "file://component_example_component1", "file://component_example_component3";
UNINSTALL COMPONENT "file://component_example_component3";
UNINSTALL COMPONENT "file://component_example_component1";
INSTALL COMPONENT "file://component_example_component3", "file://component_example_component1";
UNINSTALL COMPONENT "file://component_example_component3";
UNINSTALL COMPONENT "file://component_example_component1";
INSTALL COMPONENT "file://component_example_component1", "file://component_example_component2", "file://component_example_component3";
UNINSTALL COMPONENT "file://component_example_component1", "file://component_example_component2", "file://component_example_component3";
# Test more dependencies and groups
INSTALL COMPONENT "file://component_example_component1", "file://component_example_component2", "file://component_example_component3";
UNINSTALL COMPONENT "file://component_example_component1", "file://component_example_component2";
ERROR HY000: Unregistration of service implementation 'greetings.example_component1' provided by component 'mysql:example_component1' failed during unloading of the component.
UNINSTALL COMPONENT "file://component_example_component3";
UNINSTALL COMPONENT "file://component_example_component2";
UNINSTALL COMPONENT "file://component_example_component1";
INSTALL COMPONENT "file://component_example_component1", "file://component_example_component3";
INSTALL COMPONENT "file://component_example_component2";
UNINSTALL COMPONENT "file://component_example_component1", "file://component_example_component2";
ERROR HY000: Unregistration of service implementation 'greetings.example_component1' provided by component 'mysql:example_component1' failed during unloading of the component.
UNINSTALL COMPONENT "file://component_example_component2";
UNINSTALL COMPONENT "file://component_example_component3";
UNINSTALL COMPONENT "file://component_example_component1";
# Test overlapping groups load/unload
INSTALL COMPONENT "file://component_example_component1";
INSTALL COMPONENT "file://component_example_component2", "file://component_example_component3";
UNINSTALL COMPONENT "file://component_example_component1", "file://component_example_component3";
UNINSTALL COMPONENT "file://component_example_component2";
INSTALL COMPONENT "file://component_example_component1";
INSTALL COMPONENT "file://component_example_component2", "file://component_example_component3";
UNINSTALL COMPONENT "file://component_example_component1", "file://component_example_component2";
ERROR HY000: Unregistration of service implementation 'greetings.example_component1' provided by component 'mysql:example_component1' failed during unloading of the component.
UNINSTALL COMPONENT "file://component_example_component1", "file://component_example_component2", "file://component_example_component3";
INSTALL COMPONENT "file://component_example_component1", "file://component_example_component2";
INSTALL COMPONENT "file://component_example_component3", "file://component_example_component2";
ERROR HY000: Cannot load component from specified URN: 'file://component_example_component2'.
INSTALL COMPONENT "file://component_example_component3";
# all three components should present
SELECT COUNT(*) FROM mysql.component;
COUNT(*)
3
UNINSTALL COMPONENT "file://component_example_component2", "file://component_example_component2";
ERROR HY000: The component with specified URN: 'file://component_example_component2' was specified in group more than once.
UNINSTALL COMPONENT "file://component_example_component1", "file://component_example_component2", "file://component_example_component3";
# zero components should present
SELECT COUNT(*) FROM mysql.component;
COUNT(*)
0
#
# Checking component persistance code. Install a component, shutdown
# and restart the server, the components should present.
#
INSTALL COMPONENT "file://component_example_component1";
INSTALL COMPONENT "file://component_example_component2", "file://component_example_component3";
# all three components should present
SELECT COUNT(*) FROM mysql.component;
COUNT(*)
3
# shutdown the server from mtr.
# restart the server.
# all three components should present
SELECT COUNT(*) FROM mysql.component;
COUNT(*)
3
UNINSTALL COMPONENT "file://component_example_component1", "file://component_example_component2", "file://component_example_component3";
# zero components should present
SELECT COUNT(*) FROM mysql.component;
COUNT(*)
0
# shutdown the server from mtr.
# restart the server.
# zero components should present
SELECT COUNT(*) FROM mysql.component;
COUNT(*)
0
# Name of component is invalid
INSTALL COMPONENT "dynamic_loader_test_lib1";
ERROR HY000: Cannot find schema in specified URN: 'dynamic_loader_test_lib1'.
INSTALL COMPONENT "file://dynamic_loader_test_lib2", "dynamic_loader_test_lib3";
ERROR HY000: Can't open shared library
UNINSTALL COMPONENT "fle://dynamic_loader_test_lib1", "file://dynamic_loader_test_lib3";
ERROR HY000: Component specified by URN 'fle://dynamic_loader_test_lib1' to unload has not been loaded before.
UNINSTALL COMPONENT "dynamic_loader_test_lib2";
ERROR HY000: Component specified by URN 'dynamic_loader_test_lib2' to unload has not been loaded before.
INSTALL COMPONENT "file://localhost/tmp/dynamic_loader_test_lib1";
ERROR HY000: Cannot load component from specified URN: 'file://localhost/tmp/dynamic_loader_test_lib1'.
INSTALL COMPONENT "http://dynamic_loader_test_lib1";
ERROR HY000: Cannot acquire scheme load service implementation for schema 'http' in specified URN: 'http://dynamic_loader_test_lib1'.
INSTALL COMPONENT "file://dynamic_loader_test_lib2", "http://dynamic_loader_test_lib3";
ERROR HY000: Can't open shared library
UNINSTALL COMPONENT "file://dynamic_loader_test_lib1", "http://dynamic_loader_test_lib3";
ERROR HY000: Component specified by URN 'file://dynamic_loader_test_lib1' to unload has not been loaded before.
UNINSTALL COMPONENT "http://dynamic_loader_test_lib2";
ERROR HY000: Component specified by URN 'http://dynamic_loader_test_lib2' to unload has not been loaded before.
#
# Bug #24528148 UNPRIVILEGED USER ABLE TO LOAD COMPONENTS TO
#               MYSQL.COMPONENT
#
CREATE USER mysqltest_u1@localhost;
INSTALL COMPONENT "file://component_example_component1";
ERROR 42000: INSERT command denied to user 'mysqltest_u1'@'localhost' for table 'component'
UNINSTALL COMPONENT "file://component_example_component1";
ERROR 42000: DELETE command denied to user 'mysqltest_u1'@'localhost' for table 'component'
DROP USER mysqltest_u1@localhost;
#
# Bug #27041374: ASSERTION `NEXT_INSERT_ID == 0' FAILED.
#
SET @@session.insert_id=42949672950;
# Must not assert
INSTALL COMPONENT
"file://component_example_component2","file://component_example_component3";
ERROR HY000: Failed to manipulate component 'file://component_example_component3' persistence data. Error code 121 from storage engine.
# reset back the insert_id and auto_increment
SET @@session.insert_id=0;
ALTER TABLE mysql.component AUTO_INCREMENT=1;
# End of 8.0 tests
