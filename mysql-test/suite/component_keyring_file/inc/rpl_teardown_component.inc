# Teardown

--echo # ----------------------------------------------------------------------
--echo # Teardown
# Remove local manifest file for current server instance
--let CURRENT_DATADIR = $PRIMARY_DATADIR 
--source include/keyring_tests/helper/instance_remove_manifest.inc
--let CURRENT_DATADIR = $REPLICA_DATADIR 
--source include/keyring_tests/helper/instance_remove_manifest.inc

# Remove keyring file
--let KEYRING_FILE_PATH = $PRIMARY_KEYRING_FILE_PATH
--source include/keyring_tests/helper/local_keyring_file_remove.inc
--let KEYRING_FILE_PATH = $REPLICA_KEYRING_FILE_PATH
--source include/keyring_tests/helper/local_keyring_file_remove.inc

# Remove local keyring config
--let CURRENT_DATADIR = $PRIMARY_DATADIR 
--source include/keyring_tests/helper/local_keyring_remove_config.inc
--let CURRENT_DATADIR = $REPLICA_DATADIR 
--source include/keyring_tests/helper/local_keyring_remove_config.inc

--source include/rpl_end.inc

# Restart server without manifest file
--source include/keyring_tests/helper/cleanup_server_with_manifest.inc
--echo # ----------------------------------------------------------------------
