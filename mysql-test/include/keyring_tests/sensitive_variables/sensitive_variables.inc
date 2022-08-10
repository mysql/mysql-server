# ===== Usage ====
# --let PLUGIN_DIR=<plugin dir path>
# --source include/keyring_tests/sensitive_variables/sensitive_variables.inc

# Tests for SENSITIVE variables

--echo #
--echo # WL#13469: secure storage for sensitive system variables
--echo #

--echo # ----------------------------------------------------------------------
--echo # 1. Tests for persisted SENSITIVE variables

--connection default
INSTALL COMPONENT "file://component_test_sensitive_system_variables";
CREATE USER wl13469_no_privilege;
--connect(conn_no_priv, localhost, wl13469_no_privilege,,)

--connection default
SET PERSIST test_component.sensitive_string_1 = 'haha';
SET PERSIST_ONLY test_component.sensitive_ro_string_1 = 'haha';

--let $expected_entries = 2
--echo
--let $assert_text= PFS table persisted_variables should show SENSITIVE variables for users with SENSITIVE_VARIABLES_OBSERVER privilege
--let $assert_cond=[SELECT COUNT(*) AS entries FROM performance_schema.persisted_variables WHERE variable_name LIKE "test_component.sensitive%", entries, 1] = $expected_entries;
--source include/assert.inc

--connection conn_no_priv
--let $expected_entries = 0
--echo
--let $assert_text= PFS table persisted_variables should not show SENSITIVE variables for users without SENSITIVE_VARIABLES_OBSERVER privilege
--let $assert_cond=[SELECT COUNT(*) AS entries FROM performance_schema.persisted_variables WHERE variable_name LIKE "test_component.sensitive%", entries, 1] = $expected_entries;
--source include/assert.inc

--connection default
--disconnect conn_no_priv
DROP USER wl13469_no_privilege;

--echo # Stop the running server.
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server 300
--source include/wait_until_disconnected.inc

--echo # Restart server with keyring component
let $restart_parameters= $PLUGIN_DIR;
--source include/start_mysqld_no_echo.inc

--connection default
SELECT a.variable_name, b.variable_value, a.variable_source FROM performance_schema.variables_info AS a, performance_schema.global_variables AS b WHERE a.variable_name = b.variable_name AND a.variable_name LIKE 'test_component.sensitive%';

SET PERSIST test_component.sensitive_string_2 = 'hoho';
SET PERSIST_ONLY test_component.sensitive_ro_string_2 = 'hoho';
SET PERSIST test_component.sensitive_string_3 = 'hoho';
SET PERSIST_ONLY test_component.sensitive_ro_string_3 = 'hoho';

--echo # Stop the running server.
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server 300
--source include/wait_until_disconnected.inc

--echo # Restart server with keyring component
let $restart_parameters= $PLUGIN_DIR;
--source include/start_mysqld_no_echo.inc

--connection default
SELECT a.variable_name, b.variable_value, a.variable_source FROM performance_schema.variables_info AS a, performance_schema.global_variables AS b WHERE a.variable_name = b.variable_name AND a.variable_name LIKE 'test_component.sensitive%';

--connection default
RESET PERSIST;

UNINSTALL COMPONENT "file://component_test_sensitive_system_variables";
let $MYSQLD_DATADIR= `select @@datadir`;
--remove_file $MYSQLD_DATADIR/mysqld-auto.cnf
--echo # ----------------------------------------------------------------------
