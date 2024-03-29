# ==== Usage ====
# --let COMPONENT_DIR=<component dir path>
# --let KEYRING_COMPONENT = <keyring without extension>
# --source include/keyring_tests/mats/keyring_encryption.inc
#
# ==== Assumptions ====
# 1. Global keyring configuration file is used

--echo # ----------------------------------------------------------------------
--echo # Setup

--echo # Stop the running server.
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server 300
--source include/wait_until_disconnected.inc

--echo # ----------------------------------------------------------------------
--echo # Keyring encryption tests
--replace_regex /[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9]/TIMESTAMP/
--exec $MYSQL_KEYRING_ENCRYPTION_TEST --component-dir=$COMPONENT_DIR --keyring=$KEYRING_COMPONENT

--echo # ----------------------------------------------------------------------
--echo # Cleanup

--echo # Restart server
--exec echo "restart: --plugin-dir=$COMPONENT_DIR"> $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--enable_reconnect
--let $wait_counter= 10000
--source include/wait_until_connected_again.inc

--echo # ----------------------------------------------------------------------
