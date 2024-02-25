# ==== Usage ====
# --let COMPONENT_DIR=<component dir path>
# --let COMPONENT_NAME=<name of the component>
# --let DESTINATION_KEYRING_COMPONENT = <name of shared library>
# --let DESTINATION_KEYRING_CONFIG_CONTENT = <config file content>
# --source include/keyring_tests/migration/migration.inc

--echo # ----------------------------------------------------------------------
--echo # Setting up Keyring UDFs

# Install Keyring UDF plugin
--replace_regex /\.dll/.so/
--eval INSTALL PLUGIN keyring_udf SONAME '$KEYRING_UDF'

--replace_regex /\.dll/.so/
--eval CREATE FUNCTION keyring_key_fetch RETURNS STRING SONAME '$KEYRING_UDF'
--replace_regex /\.dll/.so/
--eval CREATE FUNCTION keyring_key_type_fetch RETURNS STRING SONAME '$KEYRING_UDF'
--replace_regex /\.dll/.so/
--eval CREATE FUNCTION keyring_key_length_fetch RETURNS INTEGER SONAME '$KEYRING_UDF'
--replace_regex /\.dll/.so/
--eval CREATE FUNCTION keyring_key_remove RETURNS INTEGER SONAME '$KEYRING_UDF'

--echo # ----------------------------------------------------------------------

--echo # Stop the running server.
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server 300
--source include/wait_until_disconnected.inc

# Take backup of manifest file to start server without keyring component
--source include/keyring_tests/helper/binary_backup_manifest.inc

# Copy keyring_file plugin data to var directory
--copy_file $MYSQLTEST_VARDIR/std_data/plugin_keyring_file_data $MYSQLTEST_VARDIR/plugin_keyring_file_data

--echo # Restart server with keyring_file plugin
--exec echo "restart: --early-plugin-load=keyring_file=$KEYRING_PLUGIN --loose-keyring_file_data=$MYSQLTEST_VARDIR/plugin_keyring_file_data"> $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--enable_reconnect
--let $wait_counter= 10000
--source include/wait_until_connected_again.inc

--connection default
--echo # Pre-migration: Content of the keyring
SELECT keyring_key_length_fetch('key1');
SELECT keyring_key_length_fetch('key2');
SELECT keyring_key_length_fetch('key3');
SELECT keyring_key_type_fetch('key1');
SELECT keyring_key_type_fetch('key2');
SELECT keyring_key_type_fetch('key3');
SELECT keyring_key_fetch('key1');
SELECT keyring_key_fetch('key2');
SELECT keyring_key_fetch('key3');

--echo # Stop the running server.
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server 300
--source include/wait_until_disconnected.inc

# Set migration source as keyring_file plugin
--let SOURCE_KEYRING_PLUGIN = $KEYRING_PLUGIN
--let SOURCE_KEYRING_OPTIONS = --loose-keyring-file-data=$MYSQLTEST_VARDIR/plugin_keyring_file_data
# Rest should have been set by caller

--echo # Perform migration
--source include/keyring_tests/helper/migrate_from_plugin_to_component.inc

# Restore manifest file
--source include/keyring_tests/helper/binary_restore_manifest.inc

--echo # Restart server with keyring component
let $restart_parameters= $PLUGIN_DIR;
--source include/start_mysqld_no_echo.inc

--connection default
SELECT status_key, status_value FROM
performance_schema.keyring_component_status
WHERE status_key LIKE '%name%' OR status_key LIKE '%status%';

--echo # Post-migration: Content of the keyring
SELECT keyring_key_length_fetch('key1');
SELECT keyring_key_length_fetch('key2');
SELECT keyring_key_length_fetch('key3');
SELECT keyring_key_type_fetch('key1');
SELECT keyring_key_type_fetch('key2');
SELECT keyring_key_type_fetch('key3');
SELECT keyring_key_fetch('key1');
SELECT keyring_key_fetch('key2');
SELECT keyring_key_fetch('key3');
SELECT keyring_key_remove('key1');
SELECT keyring_key_remove('key2');
SELECT keyring_key_remove('key3');

# Remove keyring_file plugin data from var directory
--remove_file $MYSQLTEST_VARDIR/plugin_keyring_file_data

--echo # ----------------------------------------------------------------------

--echo # Clean-up
DROP FUNCTION keyring_key_fetch;
DROP FUNCTION keyring_key_type_fetch;
DROP FUNCTION keyring_key_length_fetch;
DROP FUNCTION keyring_key_remove;
UNINSTALL PLUGIN keyring_udf;
--echo # ----------------------------------------------------------------------

