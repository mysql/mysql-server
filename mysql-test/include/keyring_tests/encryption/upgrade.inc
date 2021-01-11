# ==== Usage ====
# --let COMPONENT_DIR=<component dir path>
# --let COMPONENT_NAME=<name of the component>
# --let DESTINATION_KEYRING_COMPONENT = <name of shared library>
# --let DESTINATION_KEYRING_CONFIG_CONTENT = <config file content>
# --source include/keyring_tests/encryption/upgrade.inc

--let SOURCE_KEYRING_PLUGIN = $KEYRING_PLUGIN

--echo # This test upgrades schema from 5.7, 8.0 to 8.0.15 where we
--echo # introduce serveral ENCRYPTION control capabilities in MySQL
--echo # (WL#12261). Specifically this test upgrades from,
--echo #
--echo #   a) 5.7.25 where, we support file-per-tablespace
--echo #      encryption and not general tablespace encryption. We upgrade from
--echo #      FRM to DD.
--echo #   b) 8.0.12 where, we support file-per-tablespace
--echo #      encryption and not general tablespace encryption. We upgrade from
--echo #      DD to DD.
--echo #   c) 8.0.13 where, we support file-per-tablespace
--echo #      encryption and general tablespace encryption. We upgrade from
--echo #      DD to DD.
--echo #
--echo # The schema used is during upgrade is as described in
--echo # upgrade_schema.test placed in respective *.zip file. The expectation
--echo # is that the encryption clause for tables and tablespaces are
--echo # stored as designed by wl12261 after the upgrade is completed.. The
--echo # results are verified by querying the mysql.tables.options and
--echo # mysql.tablespaces.options.

--echo ########################################################################
--echo # Stop the running server.
--echo ########################################################################
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server 300
--source include/wait_until_disconnected.inc

# Backup keyring data file
--source include/keyring_tests/helper/local_keyring_file_backup.inc

--echo ########################################################################
--echo # Unzip a 5.7.25 datadir with bug data, start the server against it
--echo ########################################################################

--echo ########################################################################
--echo # Copy and unzip the datadir.
--echo ########################################################################
--copy_file $MYSQLTEST_VARDIR/std_data/upgrade/wl12261_upgrade_57025.zip $MYSQL_TMP_DIR/wl12261_upgrade_57025.zip
--file_exists $MYSQL_TMP_DIR/wl12261_upgrade_57025.zip
--exec unzip -qo $MYSQL_TMP_DIR/wl12261_upgrade_57025.zip -d $MYSQL_TMP_DIR
let $BUGDATA_57025_DATADIR = $MYSQL_TMP_DIR/data_57025;

--echo ########################################################################
--echo # Migrate keys from plugin to component.
--echo ########################################################################
# Take backup of global manifest file
--source include/keyring_tests/helper/binary_backup_manifest.inc
--let $SOURCE_KEYRING_OPTIONS=--loose-keyring_file_data=$BUGDATA_57025_DATADIR/mysecret_keyring
--source include/keyring_tests/helper/migrate_from_plugin_to_component.inc
# Restore global manifest file
--source include/keyring_tests/helper/binary_restore_manifest.inc

--echo ########################################################################
--echo # Restart the server against the unzipped datadir.
--echo ########################################################################
let $MYSQLD_LOG= $MYSQLTEST_VARDIR/log/save_dd_upgrade_57025.log;
--replace_regex /$PLUGIN_DIR_OPT/PLUGIN_DIR_OPT/
--exec echo "restart: $PLUGIN_DIR_OPT --datadir=$BUGDATA_57025_DATADIR --log-error=$MYSQLD_LOG" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--enable_reconnect
--let $wait_counter= 10000
--source include/wait_until_connected_again.inc

--source include/mysql_upgrade_cleanup.inc

SET DEBUG='+d,skip_dd_table_access_check';
SELECT schema_id, t.name, t.options, t.tablespace_id
  FROM mysql.tables t JOIN mysql.schemata sch
       ON t.schema_id = sch.id WHERE sch.name LIKE 'db1' ORDER BY t.name;
SELECT id, name, options FROM mysql.tablespaces
  WHERE name LIKE 'ts%' OR name LIKE '%db1%';
SET DEBUG='-d,skip_dd_table_access_check';

--echo Check if upgrade root gets TABLE_ENCRYPTION_ADMIN privilege.
SHOW GRANTS FOR 'root'@'localhost';

--echo ########################################################################
--echo # Remove copied files and stop the server.
--echo ########################################################################
--let $shutdown_server_timeout = 300
--source include/shutdown_mysqld.inc
--file_exists $MYSQL_TMP_DIR/wl12261_upgrade_57025.zip
--force-rmdir $MYSQL_TMP_DIR/data_57025
--remove_file $MYSQL_TMP_DIR/wl12261_upgrade_57025.zip
# Remove keyring file
--source include/keyring_tests/helper/local_keyring_file_remove.inc

--echo ########################################################################
--echo # Unzip a 8.0.12 datadir with bug data, start the server against it
--echo ########################################################################

--echo ########################################################################
--echo # Copy and unzip the datadir.
--echo ########################################################################
--copy_file $MYSQLTEST_VARDIR/std_data/upgrade/wl12261_upgrade_80012.zip $MYSQL_TMP_DIR/wl12261_upgrade_80012.zip
--file_exists $MYSQL_TMP_DIR/wl12261_upgrade_80012.zip
--exec unzip -qo $MYSQL_TMP_DIR/wl12261_upgrade_80012.zip -d $MYSQL_TMP_DIR
let $BUGDATA_80012_DATADIR = $MYSQL_TMP_DIR/data_80012;

--echo ########################################################################
--echo # Migrate keys from plugin to component.
--echo ########################################################################
# Take backup of global manifest file
--source include/keyring_tests/helper/binary_backup_manifest.inc
--let $SOURCE_KEYRING_OPTIONS=--loose-keyring_file_data=$BUGDATA_80012_DATADIR/mysecret_keyring
--source include/keyring_tests/helper/migrate_from_plugin_to_component.inc
# Restore global manifest file
--source include/keyring_tests/helper/binary_restore_manifest.inc

--echo ########################################################################
--echo # Restart the server against the unzipped datadir.
--echo ########################################################################
let $MYSQLD_LOG= $MYSQLTEST_VARDIR/log/save_dd_upgrade_80012.log;
--replace_regex /$PLUGIN_DIR_OPT/PLUGIN_DIR_OPT/
--exec echo "restart: $PLUGIN_DIR_OPT --datadir=$BUGDATA_80012_DATADIR --log-error=$MYSQLD_LOG" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--enable_reconnect
--let $wait_counter= 10000
--source include/wait_until_connected_again.inc

--source include/mysql_upgrade_cleanup.inc

SET DEBUG='+d,skip_dd_table_access_check';
SELECT schema_id, t.name, t.options, t.tablespace_id
  FROM mysql.tables t JOIN mysql.schemata sch
       ON t.schema_id = sch.id WHERE sch.name LIKE 'db1' ORDER BY t.name;
SELECT id, name, options FROM mysql.tablespaces
  WHERE name LIKE 'ts%' OR name LIKE '%db1%';
SET DEBUG='-d,skip_dd_table_access_check';

--echo Check if upgrade root gets TABLE_ENCRYPTION_ADMIN privilege.
SHOW GRANTS FOR 'root'@'localhost';

--echo ########################################################################
--echo # Remove copied files and stop the server.
--echo ########################################################################
--let $shutdown_server_timeout = 300
--source include/shutdown_mysqld.inc
--file_exists $MYSQL_TMP_DIR/wl12261_upgrade_80012.zip
--force-rmdir $MYSQL_TMP_DIR/data_80012
--remove_file $MYSQL_TMP_DIR/wl12261_upgrade_80012.zip
# Remove keyring file
--source include/keyring_tests/helper/local_keyring_file_remove.inc

--echo ########################################################################
--echo # Unzip a 8.0.13 datadir with bug data, start the server against it
--echo ########################################################################

--echo ########################################################################
--echo # Copy and unzip the datadir.
--echo ########################################################################
--copy_file $MYSQLTEST_VARDIR/std_data/upgrade/wl12261_upgrade_80013.zip $MYSQL_TMP_DIR/wl12261_upgrade_80013.zip
--file_exists $MYSQL_TMP_DIR/wl12261_upgrade_80013.zip
--exec unzip -qo $MYSQL_TMP_DIR/wl12261_upgrade_80013.zip -d $MYSQL_TMP_DIR
let $BUGDATA_80013_DATADIR = $MYSQL_TMP_DIR/data_80013;

--echo ########################################################################
--echo # Migrate keys from plugin to component.
--echo ########################################################################
# Take backup of global manifest file
--source include/keyring_tests/helper/binary_backup_manifest.inc
--let $SOURCE_KEYRING_OPTIONS=--loose-keyring_file_data=$BUGDATA_80013_DATADIR/mysecret_keyring
--source include/keyring_tests/helper/migrate_from_plugin_to_component.inc
# Restore global manifest file
--source include/keyring_tests/helper/binary_restore_manifest.inc

--echo ########################################################################
--echo # Restart the server against the unzipped datadir.
--echo ########################################################################
let $MYSQLD_LOG= $MYSQLTEST_VARDIR/log/save_dd_upgrade_80013.log;
--replace_regex /$PLUGIN_DIR_OPT/PLUGIN_DIR_OPT/
--exec echo "restart: $PLUGIN_DIR_OPT --datadir=$BUGDATA_80013_DATADIR --log-error=$MYSQLD_LOG" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--enable_reconnect
--let $wait_counter= 10000
--source include/wait_until_connected_again.inc

--source include/mysql_upgrade_cleanup.inc

SET DEBUG='+d,skip_dd_table_access_check';
SELECT schema_id, t.name, t.options, t.tablespace_id
  FROM mysql.tables t JOIN mysql.schemata sch
       ON t.schema_id = sch.id WHERE sch.name LIKE 'db1' ORDER BY t.name;
SELECT id, name, options FROM mysql.tablespaces
  WHERE name LIKE 'ts%' OR name LIKE '%db1%';
SET DEBUG='-d,skip_dd_table_access_check';

--echo Check if upgrade root gets TABLE_ENCRYPTION_ADMIN privilege.
SHOW GRANTS FOR 'root'@'localhost';

--echo ########################################################################
--echo # Remove copied files and stop the server.
--echo ########################################################################
--let $shutdown_server_timeout = 300
--source include/shutdown_mysqld.inc
--file_exists $MYSQL_TMP_DIR/wl12261_upgrade_80013.zip
--force-rmdir $MYSQL_TMP_DIR/data_80013
--remove_file $MYSQL_TMP_DIR/wl12261_upgrade_80013.zip
# Remove keyring file
--source include/keyring_tests/helper/local_keyring_file_remove.inc

--echo ########################################################################
--echo # Cleanup: Restart with default options.
--echo ########################################################################
# Restore keyring data file
--source include/keyring_tests/helper/local_keyring_file_restore.inc
let $restart_parameters =;
--source include/start_mysqld_no_echo.inc

--echo ########################################################################
--echo # Bug #29447073: UPGRADE FROM 8.0.16 TO TRUNK FAILS WHEN MYSQL TABLESPACE IS ENCRYPTED
--echo ########################################################################

alter tablespace mysql encryption='Y';

--let $restart_parameters = restart: $PLUGIN_DIR_OPT --upgrade=FORCE
--let $wait_counter=10000
--source include/restart_mysqld_no_echo.inc
--source include/wait_until_connected_again.inc

--source include/mysql_upgrade_cleanup.inc

alter tablespace mysql encryption='N';

--echo ########################################################################
--echo # Cleanup: Restart with default options.
--echo ########################################################################
let $restart_parameters =;
--source include/start_mysqld_no_echo.inc

