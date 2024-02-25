# ==== Usage ====
# --let COMPONENT_DIR=<component dir path>
# --let COMPONENT_NAME=<name of the component>
# --let DESTINATION_KEYRING_COMPONENT = <name of shared library>
# --let DESTINATION_KEYRING_CONFIG_CONTENT = <config file content>
# --source include/keyring_tests/encryption/dd_upgrade_encrypted.inc

--let SOURCE_KEYRING_PLUGIN = $KEYRING_PLUGIN

--echo ########################################################################
--echo # Bug#29491749: DATA DICTIONARY UPGRADE FAILING WITH ENCRYPTED MYSQL
--echo # TABLESPACE
--echo #
--echo # Verify that we can upgrade from a server with an encrypted mysql
--echo # tablespace.
--echo ########################################################################

--echo # Stop the mysqld server which was created by MTR default
--let $shutdown_server_timeout= 300
--source include/shutdown_mysqld.inc

# Backup keyring data file
--source include/keyring_tests/helper/local_keyring_file_backup.inc

--echo # Copy and unzip the datadir
--copy_file $MYSQLTEST_VARDIR/std_data/upgrade/data_encrypted_80016.zip $MYSQL_TMP_DIR/data_encrypted_80016.zip
--file_exists $MYSQL_TMP_DIR/data_encrypted_80016.zip
--exec unzip -qo $MYSQL_TMP_DIR/data_encrypted_80016.zip -d $MYSQL_TMP_DIR

--echo # Set different datadir path
--let $MYSQLD_DATADIR= $MYSQL_TMP_DIR/data_encrypted
--let $MYSQLD_LOG= $MYSQLTEST_VARDIR/log/upgrade_encrypted.log

--echo ########################################################################
--echo # Migrate keys from plugin to component.
--echo ########################################################################
# Take backup of global manifest file
--source include/keyring_tests/helper/binary_backup_manifest.inc
--let $SOURCE_KEYRING_OPTIONS=--loose-keyring_file_data=$MYSQLD_DATADIR/mysecret_keyring
--source include/keyring_tests/helper/migrate_from_plugin_to_component.inc
# Restore global manifest file
--source include/keyring_tests/helper/binary_restore_manifest.inc

--echo # Start the server
--let $restart_parameters= restart: $PLUGIN_DIR_OPT --datadir=$MYSQLD_DATADIR --log-error=$MYSQLD_LOG
--let $wait_counter= 10000
--let $shutdown_server_timeout= 300
--source include/start_mysqld_no_echo.inc

SET debug='+d,skip_dd_table_access_check';

--echo # The mysql tablespace should be encrypted
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME='mysql';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE NAME='mysql';

--echo # Verify that we can un-encrypt the mysql tablespace
ALTER TABLESPACE mysql ENCRYPTION='N';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME='mysql';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE NAME='mysql';

--echo # Restart the server
--let $restart_parameters= restart: $PLUGIN_DIR_OPT --datadir=$MYSQLD_DATADIR --log-error=$MYSQLD_LOG
--let $wait_counter= 10000
--let $shutdown_server_timeout= 300
--source include/restart_mysqld_no_echo.inc

SET debug='+d,skip_dd_table_access_check';

--echo # The mysql tablespace should now be be un-encrypted
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME='mysql';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE NAME='mysql';

--echo # Shutdown the server, remove the files, and restart with default options
--let $wait_counter= 10000
--let $shutdown_server_timeout= 300
--source include/shutdown_mysqld.inc

--remove_file $MYSQL_TMP_DIR/data_encrypted_80016.zip
--force-rmdir $MYSQL_TMP_DIR/data_encrypted
# Remove keyring file
--source include/keyring_tests/helper/local_keyring_file_remove.inc
# Restore keyring data file
--source include/keyring_tests/helper/local_keyring_file_restore.inc

--echo # Restart the server with default options
--let $restart_parameters= restart;
--let $wait_counter= 10000
--source include/start_mysqld.inc
