# ==== Usage ====
# --let COMPONENT_DIR=<component dir path>
# --let COMPONENT_NAME=<name of the component>
# --let SOURCE_KEYRING_COMPONENT = <name of shared library>
# --let SOURCE_KEYRING_CONFIG_CONTENT = <config file content>
# --source include/keyring_tests/migration/migrate_to_plugin.inc

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
--replace_regex /\.dll/.so/
--eval CREATE FUNCTION keyring_key_store RETURNS INTEGER SONAME '$KEYRING_UDF'
--replace_regex /\.dll/.so/
--eval CREATE FUNCTION keyring_key_generate RETURNS INTEGER SONAME '$KEYRING_UDF'

--echo # -----------------------------------------------------------------
--echo # Create Keys
SELECT keyring_key_generate('gkey16','AES',16);
SELECT keyring_key_generate('gkey24','AES',24);
SELECT keyring_key_generate('gkey32','AES',32);

--echo # Verify Keys
SELECT keyring_key_type_fetch('gkey16');
SELECT keyring_key_type_fetch('gkey24');
SELECT keyring_key_type_fetch('gkey32');
SELECT keyring_key_length_fetch('gkey16');
SELECT keyring_key_length_fetch('gkey24');
SELECT keyring_key_length_fetch('gkey32');
let $gkey16_s = `SELECT HEX(keyring_key_fetch('gkey16'))`;
let $gkey24_s = `SELECT HEX(keyring_key_fetch('gkey24'))`;
let $gkey32_s = `SELECT HEX(keyring_key_fetch('gkey32'))`;

--echo # Stop the running server.
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server 300
--source include/wait_until_disconnected.inc

# Take backup of manifest file to perform migration
--source include/keyring_tests/helper/binary_backup_manifest.inc

# Set migration source as keyring_aws plugin
--let DESTINATION_KEYRING_PLUGIN = $KEYRING_AWS_PLUGIN
--replace_result $KEYRING_AWS_PLUGIN_OPT KEYRING_AWS_PLUGIN_OPT $KEYRING_UDF_CONF KEYRING_UDF_CONF $MYSQL_TMP_DIR MYSQL_TMP_DIR $KEYRING_UDF_CMK_ID_1 KEYRING_UDF_CMK_ID_1 .dll .so
--let DESTINATION_KEYRING_OPTIONS = --loose-keyring_aws_conf_file=$KEYRING_UDF_CONF --loose-keyring_aws_data_file=$MYSQL_TMP_DIR/keyring_aws/keyring_aws --loose-keyring_aws_cmk_id=$KEYRING_UDF_CMK_ID_1
# Rest should have been set by caller

--echo # Perform migration
--source include/keyring_tests/helper/migrate_from_component_to_plugin.inc

--echo # Restart mysql server with keyring_aws plugin.
--replace_result $KEYRING_AWS_PLUGIN_OPT KEYRING_AWS_PLUGIN_OPT $KEYRING_UDF_CONF KEYRING_UDF_CONF $MYSQL_TMP_DIR MYSQL_TMP_DIR $KEYRING_UDF_CMK_ID_1 KEYRING_UDF_CMK_ID_1 .dll .so
--let $restart_parameters=restart:--early-plugin-load="keyring_aws=$KEYRING_AWS_PLUGIN" --loose-keyring_aws_conf_file=$KEYRING_UDF_CONF $KEYRING_AWS_PLUGIN_OPT --loose-keyring_aws_data_file=$MYSQL_TMP_DIR/keyring_aws/keyring_aws --loose-keyring_aws_cmk_id=$KEYRING_UDF_CMK_ID_1
--source include/start_mysqld_no_echo.inc

--echo # Verify Key Migration
SELECT keyring_key_type_fetch('gkey16');
SELECT keyring_key_type_fetch('gkey24');
SELECT keyring_key_type_fetch('gkey32');
SELECT keyring_key_length_fetch('gkey16');
SELECT keyring_key_length_fetch('gkey24');
SELECT keyring_key_length_fetch('gkey32');
let $gkey16_d = `SELECT HEX(keyring_key_fetch('gkey16'))`;
let $gkey24_d = `SELECT HEX(keyring_key_fetch('gkey24'))`;
let $gkey32_d = `SELECT HEX(keyring_key_fetch('gkey32'))`;
--replace_result $gkey16_s <gkey16_s> $gkey16_d <gkey16_d>
eval SELECT '$gkey16_s' = '$gkey16_d';
--replace_result $gkey24_s <gkey24_s> $gkey24_d <gkey24_d>
eval SELECT '$gkey24_s' = '$gkey24_d';
--replace_result $gkey32_s <gkey32_s> $gkey32_d <gkey32_d>
eval SELECT '$gkey32_s' = '$gkey32_d';

# Remove Keys
SELECT keyring_key_remove('gkey16');
SELECT keyring_key_remove('gkey24');
SELECT keyring_key_remove('gkey32');

# Restore manifest file
--source include/keyring_tests/helper/binary_restore_manifest.inc

--echo # Restart server with keyring component
--source include/keyring_tests/helper/start_server_with_manifest.inc

# Remove Keys
SELECT keyring_key_remove('gkey16');
SELECT keyring_key_remove('gkey24');
SELECT keyring_key_remove('gkey32');

--echo # ----------------------------------------------------------------------

--echo # Clean-up
DROP FUNCTION keyring_key_fetch;
DROP FUNCTION keyring_key_type_fetch;
DROP FUNCTION keyring_key_length_fetch;
DROP FUNCTION keyring_key_store;
DROP FUNCTION keyring_key_remove;
DROP FUNCTION keyring_key_generate;
UNINSTALL PLUGIN keyring_udf;
--echo # ----------------------------------------------------------------------

