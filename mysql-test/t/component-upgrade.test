--source include/have_example_component.inc
--source include/not_valgrind.inc

# Save timestamp to restore tables
--let $PWCHANGED= `SELECT password_last_changed FROM mysql.user WHERE user LIKE 'mysql.sys';`
--let $TIMESTAMP= `SELECT timestamp FROM mysql.tables_priv WHERE user LIKE 'mysql.sys';`

--echo #
--echo # Bug #24453571  SERVER CRASHES WHEN INSTALL COMPONENT IS ISSUED,
--echo # AFTER MYSQL_UPGRADE
--echo #

--echo # Checking MYSQL_UPGRADE creates the missing mysql.component table
--echo # when upgrading from 5.7. Simulating the senario by deleting the
--echo # component table.

CALL mtr.add_suppression("Table 'mysql.component' doesn't exis");
CALL mtr.add_suppression("The mysql.component table is missing or has an incorrect definition.");

--echo # Dropping component table.
DROP TABLE mysql.component;
--echo # shutdown the server from mtr.
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc

--echo # restart the server.
--exec echo "restart:" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc
--echo # running mysql_upgrade
# Filter out ndb_binlog_index to mask differences due to running with or without
# ndb.
--let $restart_parameters = restart:--upgrade=FORCE
--let $wait_counter= 10000
--source include/restart_mysqld.inc

--source include/have_example_component.inc
INSTALL COMPONENT "file://component_example_component1";
--echo # a component should present
SELECT COUNT(*) FROM mysql.component;
UNINSTALL COMPONENT "file://component_example_component1";

# Restore user tables
--disable_query_log
--eval UPDATE mysql.user SET password_last_changed = '$PWCHANGED' WHERE user LIKE 'mysql.sys'
--eval UPDATE mysql.tables_priv SET timestamp = '$TIMESTAMP' WHERE user LIKE 'mysql.sys'
--enable_query_log

# restore default options in .opt
--let $restart_parameters = restart:
--source include/restart_mysqld.inc
