# Bug #24660436 MYSQL.COMPONENT HAVE MULTIPLE ROWS AFTER
#               CANCELLING INSTALL/UNINSTALL WITH CTRL+C
INSTALL COMPONENT "file://component_example_component1", "file://component_example_component3", "file://component_example_component2";
SET DEBUG_SYNC='before_ha_index_read_idx_map SIGNAL kill_query WAIT_FOR nothing TIMEOUT 10';
UNINSTALL COMPONENT "file://component_example_component1", "file://component_example_component3", "file://component_example_component2";
SET DEBUG_SYNC='now WAIT_FOR kill_query';
KILL QUERY ID;;
ERROR 70100: Query execution was interrupted
Since UNINSTALL component failed because of "Query execution
was interrupted" error. This should display three components.
SELECT COUNT(*) FROM mysql.component;
COUNT(*)
3
Below INSTALL component should fail after the fix
INSTALL COMPONENT "file://component_example_component1", "file://component_example_component3", "file://component_example_component2";
ERROR HY000: Cannot load component from specified URN: 'file://component_example_component1'.
Should display three components
SELECT COUNT(*) FROM mysql.component;
COUNT(*)
3
UNINSTALL COMPONENT "file://component_example_component1", "file://component_example_component3", "file://component_example_component2";
