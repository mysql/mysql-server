--source include/have_debug.inc

--echo #
--echo # WL#13469: secure storage for sensitive system variables
--echo #

--disable_query_log
call mtr.add_suppression("Persisting SENSITIVE variables in encrypted form requires keyring component loaded through manifest file.");
call mtr.add_suppression("Cannot persist SENSITIVE system variables because keyring component support is unavailable and persist_sensitive_variables_in_plaintext is set to OFF.");
--enable_query_log

INSTALL COMPONENT 'file://component_test_sensitive_system_variables';

--echo # ----------------------------------------------------------------------
--echo # 1. If persist_sensitive_variables_in_plaintext is
--echo #    set to FALSE/OFF and keyring component is not
--echo #    available, trying to persist a SENSITIVE variable
--echo #    should result into error.

--echo # With --persist_sensitive_variables_in_plaintext set
--echo # to FALSE and keyring component not configured,
--echo # trying to persist SENSITIVE variables should fail.

--connection default
SELECT @@global.persist_sensitive_variables_in_plaintext;

--error ER_CANNOT_PERSIST_SENSITIVE_VARIABLES
SET PERSIST test_component.sensitive_string_1 = 'haha';

--error ER_CANNOT_PERSIST_SENSITIVE_VARIABLES
SET PERSIST_ONLY test_component.sensitive_string_1 = 'haha';

--error ER_CANNOT_PERSIST_SENSITIVE_VARIABLES
SET PERSIST_ONLY test_component.sensitive_ro_string_1 = 'haha';

--echo # ----------------------------------------------------------------------
--echo # 2. If persist_sensitive_variables_in_plaintext is set
--echo #    to TRUE/ON and keyring component support is not
--echo #    available, it should possible to persist
--echo #    SENSITIVE variables' values.

--echo # Stop the running server.
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server 300
--source include/wait_until_disconnected.inc

--echo # Restart server without keyiring and with persist_sensitive_variables_in_plaintext=TRUE
--exec echo "restart: --persist_sensitive_variables_in_plaintext=TRUE"> $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--let $wait_counter= 10000
--source include/wait_until_connected_again.inc

--echo # With --persist_sensitive_variables_in_plaintext set
--echo # to TRUE, even if keyring component not configured,
--echo # trying to persist SENSITIVE variable should succeed.

--connection default

SELECT @@global.persist_sensitive_variables_in_plaintext;

SET PERSIST test_component.sensitive_string_1 = 'haha';

SET PERSIST_ONLY test_component.sensitive_string_1 = 'haha';

SET PERSIST_ONLY test_component.sensitive_ro_string_1 = 'haha';

--echo # Stop the running server.
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server 300
--source include/wait_until_disconnected.inc

--echo # Restart server without keyiring and with persist_sensitive_variables_in_plaintext=TRUE
--exec echo "restart: --persist_sensitive_variables_in_plaintext=TRUE"> $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--let $wait_counter= 10000
--source include/wait_until_connected_again.inc

--connection default
SELECT a.variable_name, b.variable_value, a.variable_source FROM performance_schema.variables_info AS a, performance_schema.global_variables AS b WHERE a.variable_name = b.variable_name AND a.variable_name LIKE 'test_component.sensitive%';

RESET PERSIST;

UNINSTALL COMPONENT 'file://component_test_sensitive_system_variables';

let $MYSQLD_DATADIR= `select @@datadir`;
--remove_file $MYSQLD_DATADIR/mysqld-auto.cnf

--source include/force_restart.inc
--echo # ----------------------------------------------------------------------
