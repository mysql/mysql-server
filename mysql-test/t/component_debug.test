--source include/have_debug_sync.inc
--source include/have_debug.inc
--source include/have_example_component.inc

--echo # Bug #24660436 MYSQL.COMPONENT HAVE MULTIPLE ROWS AFTER
--echo #               CANCELLING INSTALL/UNINSTALL WITH CTRL+C

let $connection_id= `SELECT CONNECTION_ID()`;
INSTALL COMPONENT "file://component_example_component1", "file://component_example_component3", "file://component_example_component2";
SET DEBUG_SYNC='before_ha_index_read_idx_map SIGNAL kill_query WAIT_FOR nothing TIMEOUT 10';
send UNINSTALL COMPONENT "file://component_example_component1", "file://component_example_component3", "file://component_example_component2";

connect(con1, localhost, root,,);
SET DEBUG_SYNC='now WAIT_FOR kill_query';
--replace_result $connection_id ID
--eval KILL QUERY $connection_id;

connection default;
--error ER_QUERY_INTERRUPTED
--reap
--echo Since UNINSTALL component failed because of "Query execution
--echo was interrupted" error. This should display three components.
SELECT COUNT(*) FROM mysql.component;
--echo Below INSTALL component should fail after the fix
--error ER_COMPONENTS_CANT_LOAD
INSTALL COMPONENT "file://component_example_component1", "file://component_example_component3", "file://component_example_component2";
--echo Should display three components
SELECT COUNT(*) FROM mysql.component;
UNINSTALL COMPONENT "file://component_example_component1", "file://component_example_component3", "file://component_example_component2";
disconnect con1;
