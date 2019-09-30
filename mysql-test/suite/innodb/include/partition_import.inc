# WL 13352: Import  partitioned tables from older version zip file
# Works for version 8.0.13, 8.0.15, 8.0.17, 8.0.19

--echo # Test to create schema for Upgrade in old version

--source include/have_innodb_16k.inc

# Check migration across platforms for 8.0.19
if ($OLD_VERSION != 8019) {
  --source include/have_case_sensitive_file_system.inc
}

# To Enable fixed path for external files
# --let $IS_FIXED_PATH = 1

--let $MYSQLD_DATADIR= `select @@datadir`
--let $EXTERNAL_DIR_NAME= mysql_wl13352_data

# For manual upgrade testing with absolute path to match across setup
if ($IS_FIXED_PATH)
{
  --let $EXTERNAL_DIRECTORY = /tmp/$EXTERNAL_DIR_NAME/
  --let $UNZIP_DIR = /
}

if (!$IS_FIXED_PATH)
{
  --let $EXTERNAL_DIRECTORY = $MYSQL_TMP_DIR/tmp/$EXTERNAL_DIR_NAME/
  --let $UNZIP_DIR = $MYSQL_TMP_DIR
}

--let $EXPORT_DIRECTORY = $MYSQL_TMP_DIR/data_export
--mkdir $EXPORT_DIRECTORY

# Cleanup external directory
--error 0, 1
--force-rmdir $EXTERNAL_DIRECTORY

--echo # Extract data files to be imported
--copy_file $MYSQLTEST_VARDIR/std_data/upgrade/export_wl13352_$OLD_VERSION.zip $MYSQL_TMP_DIR/data_export.zip
--file_exists $MYSQL_TMP_DIR/data_export.zip
--exec unzip -qo $MYSQL_TMP_DIR/data_export.zip -d $UNZIP_DIR
--remove_file $MYSQL_TMP_DIR/data_export.zip

--echo # Starting server with keyring plugin
--let $restart_parameters = restart: --innodb_directories=$EXTERNAL_DIRECTORY --early-plugin-load="keyring_file=$KEYRING_PLUGIN" --loose-keyring_file_data=$MYSQL_TMP_DIR/mysecret_keyring  $KEYRING_PLUGIN_OPT
--replace_result $MYSQL_TMP_DIR MYSQL_TMP_DIR $KEYRING_PLUGIN_OPT --plugin-dir=KEYRING_PLUGIN_PATH $KEYRING_PLUGIN keyring_file.so $EXTERNAL_DIRECTORY /tmp/mysql_wl13352_data/
--source include/restart_mysqld.inc

--echo
--echo # A. Create tables and fill data

--let $check_option = CREATE
--source suite/innodb/include/partition_upgrade_check.inc

--echo
--echo # Import table partitions
--let $check_option = IMPORT
--source suite/innodb/include/partition_upgrade_check.inc
--let $imported = 1

--echo
--echo # Check table metadata and data
--let $check_option = CHECK
--source suite/innodb/include/partition_upgrade_check.inc

--echo
--echo # Drop all tables created for import
--let $check_option = DROP
--source suite/innodb/include/partition_upgrade_check.inc

--echo
--echo # Stop DB server
--source include/shutdown_mysqld.inc

--echo # Cleanup: Restart with default options.
let $restart_parameters =;
--source include/start_mysqld.inc

--force-rmdir $EXTERNAL_DIRECTORY
--force-rmdir $EXPORT_DIRECTORY
if (!$IS_FIXED_PATH)
{
  --force-rmdir $MYSQL_TMP_DIR/tmp
}
--remove_file $MYSQL_TMP_DIR/mysecret_keyring
