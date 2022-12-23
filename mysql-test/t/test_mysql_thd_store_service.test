--source include/have_component_test_mysql_thd_store_service.inc

--echo # Test for mysql_thd_store service

--echo # Save the initial number of concurrent sessions
--source include/count_sessions.inc

--enable_connect_log
--connect(test_connection, localhost, root)

--echo # This would store data in THD for test_connection
INSTALL COMPONENT "file://component_test_mysql_thd_store_service";

--connect(test_connection2, localhost, root)

SELECT test_thd_store_service_function();

--connection default
--echo # This would trigger callback to component to free resouces
--echo # stored by the component
--disconnect test_connection2

--echo # Must fail because required data won't be available in THD for default
--error ER_COMPONENTS_UNLOAD_CANT_DEINITIALIZE
UNINSTALL COMPONENT "file://component_test_mysql_thd_store_service";

--connection test_connection
--echo # Must succeed because required data should
--echo # be available in THD for test_connection
UNINSTALL COMPONENT "file://component_test_mysql_thd_store_service";

--connection default
--disconnect test_connection

--disable_connect_log

--echo # Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc
