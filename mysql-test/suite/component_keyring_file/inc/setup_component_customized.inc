# Setup test to use keyring component

--echo # ----------------------------------------------------------------------
--echo # Setup

--let PLUGIN_DIR_OPT = $KEYRING_FILE_COMPONENT_OPT

--let COMPONENT_LIBRARY = `SELECT SUBSTRING_INDEX('$KEYRING_FILE_COMPONENT_LOAD', '=', -1)`
--let COMPONENT_DIR = $KEYRING_FILE_COMPONENT_DIR
--let COMPONENT_NAME = `SELECT SUBSTRING_INDEX('$COMPONENT_LIBRARY', '.', 1)`
--let GLOBAL_MANIFEST_CONTENT = `SELECT CONCAT('{ \"components\": \"file://', '$COMPONENT_NAME', '\" }')`
# Create manifest file for mysqld binary
--source include/keyring_tests/helper/binary_create_customized_manifest.inc

# Create global keyring config
--let KEYRING_FILE_PATH = `SELECT CONCAT( '$MYSQLTEST_VARDIR', '/keyring_file')`
--let KEYRING_CONFIG_CONTENT = `SELECT CONCAT('{ \"path\": \"', '$KEYRING_FILE_PATH','\", \"read_only\": false }')`
--source include/keyring_tests/helper/global_keyring_create_customized_config.inc

# Restart server with manifest file 
--source include/keyring_tests/helper/start_server_with_manifest.inc
--echo # ----------------------------------------------------------------------

