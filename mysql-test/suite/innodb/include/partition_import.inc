# WL 13352: Import  partitioned tables from older version zip file
# Works for version 8.0.13, 8.0.15, 8.0.17, 8.0.19

# To Enable fixed path for external files
# --let $IS_FIXED_PATH = 1

--echo # Test to create schema for Upgrade in old version

--source include/big_test.inc
--source include/not_parallel.inc
--source include/have_innodb_16k.inc
--source include/no_valgrind_without_big.inc
--source include/have_component_keyring_file.inc

--disable_query_log
CALL mtr.add_suppression("Component component_keyring_file reported:");
--enable_query_log

# Check import across platforms for 8.0.19
if ($OLD_VERSION != 8019) {
  --source include/have_case_sensitive_file_system.inc
  --let $OLD_LCTN = 0
}

if ($OLD_LCTN == 0)
{
  --source include/have_lowercase0.inc
}

if ($OLD_LCTN == 1)
{
  --source include/have_lowercase1.inc
}

if ($OLD_LCTN == 2)
{
  --source include/have_lowercase2.inc
}

--let $OLD_EXTN = $OLD_VERSION

# Check migration across platforms for 8.0.19
if ($OLD_VERSION == 8019)
{
  --let $OLD_EXTN = `select concat("$OLD_VERSION", "_", "$OLD_PLATFORM", "_lctn_", "$OLD_LCTN")`
}

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
--copy_file $MYSQLTEST_VARDIR/std_data/upgrade/export_wl13352_$OLD_EXTN.zip $MYSQL_TMP_DIR/data_export.zip
--file_exists $MYSQL_TMP_DIR/data_export.zip
--exec unzip -qo $MYSQL_TMP_DIR/data_export.zip -d $UNZIP_DIR
--remove_file $MYSQL_TMP_DIR/data_export.zip

--echo # Starting server with keyring plugin
--source ../mysql-test/suite/component_keyring_file/inc/setup_component_customized.inc
--let $restart_parameters = restart: --innodb_directories=$EXTERNAL_DIRECTORY $PLUGIN_DIR_OPT
--replace_result $MYSQL_TMP_DIR MYSQL_TMP_DIR $PLUGIN_DIR_OPT --plugin-dir=PLUGIN_DIR $EXTERNAL_DIRECTORY /tmp/mysql_wl13352_data/
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
--source ../mysql-test/suite/component_keyring_file/inc/teardown_component_customized.inc

