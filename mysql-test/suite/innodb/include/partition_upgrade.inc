# WL 13352: Test Upgrade of partitioned tables from different versions
# Caller need to set OLD_VERSION: 5727, 8013, 8015, 8017, 8019

--echo # Test Upgrade from old version $OLD_VERSION

--source include/have_innodb_16k.inc

# Check migration across platforms for 8.0.19 [lower_case_table_names = 1]
if ($OLD_VERSION != 8019) {
  --source include/have_case_sensitive_file_system.inc
}

--echo # Stop DB server which was created by MTR default
--source include/shutdown_mysqld.inc

# To disable external file path
# --let $NO_EXT_PATH = 1

# To Enable fixed path for external files
# --let $IS_FIXED_PATH = 1

# By default skip external directory for 5.7
if ($OLD_VERSION == '5727')
{
  --let $NO_EXT_PATH = 1
}

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

# Cleanup external directory
--error 0, 1
--force-rmdir $EXTERNAL_DIRECTORY

--echo # Copy and unzip old version data directory.
--copy_file $MYSQLTEST_VARDIR/std_data/upgrade/data_wl13352_$OLD_VERSION.zip $MYSQL_TMP_DIR/data_upgrade.zip
--file_exists $MYSQL_TMP_DIR/data_upgrade.zip
--exec unzip -qo $MYSQL_TMP_DIR/data_upgrade.zip -d $MYSQL_TMP_DIR
--remove_file $MYSQL_TMP_DIR/data_upgrade.zip

if (!$NO_EXT_PATH)
{
  --echo # Copy and unzip external directory
  --copy_file $MYSQLTEST_VARDIR/std_data/upgrade/extn_wl13352_$OLD_VERSION.zip $MYSQL_TMP_DIR/data_extn.zip
  --file_exists $MYSQL_TMP_DIR/data_extn.zip
  --exec unzip -qo $MYSQL_TMP_DIR/data_extn.zip -d $UNZIP_DIR
  --remove_file $MYSQL_TMP_DIR/data_extn.zip
}

let $MYSQLD_DATADIR_UPGRADE = $MYSQL_TMP_DIR/data_upgrade;
--echo List all files before upgrade
--echo -----------------------------
--list_files $MYSQLD_DATADIR_UPGRADE/test

if (!$NO_EXT_PATH)
{
  --echo
  --echo List all files in external directory
  --echo ------------------------------------
  --list_files $EXTERNAL_DIRECTORY/test

  --echo
  --echo List files outside database directory (empty)
  --echo ---------------------------------------------
  --list_files $EXTERNAL_DIRECTORY
}

--echo
--echo # Upgrade to latest mysql version
let $MYSQLD_LOG= $MYSQLTEST_VARDIR/log/mysqld_upgrade.log;

if ($NO_EXT_PATH) {
  --replace_result $MYSQLD MYSQLD $MYSQLD_DATADIR_UPGRADE MYSQLD_DATADIR_UPGRADE $MYSQLD_LOG MYSQLD_LOG
  --exec echo "restart: --datadir=$MYSQLD_DATADIR_UPGRADE --lower_case_table_names=1 --log-error=$MYSQLD_LOG" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
}

if (!$NO_EXT_PATH) {
  --replace_result $MYSQLD MYSQLD $MYSQLD_DATADIR_UPGRADE MYSQLD_DATADIR_UPGRADE $MYSQLD_LOG MYSQLD_LOG $EXTERNAL_DIRECTORY EXTERNAL_DIRECTORY
  --exec echo "restart: --datadir=$MYSQLD_DATADIR_UPGRADE --lower_case_table_names=1 --innodb_directories=$EXTERNAL_DIRECTORY --log-error=$MYSQLD_LOG" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
}

--let $wait_counter= 10000
--enable_reconnect
--source include/wait_until_connected_again.inc

--echo
--echo List all files of upgraded data
--echo -------------------------------
--list_files $MYSQLD_DATADIR_UPGRADE/test

if (!$NO_EXT_PATH)
{
  --echo
  --echo List all files in external directory
  --echo ------------------------------------
  --list_files $EXTERNAL_DIRECTORY/test

  --echo
  --echo List files outside database directory (empty)
  --echo ---------------------------------------------
  --list_files $EXTERNAL_DIRECTORY
}

--echo
--echo # A. Validate upgraded tables and data

--let $i = 0

while($i < 2)
{
  --echo # 1. Check table metadata and data
  --let $check_option = CHECK
  --source partition_upgrade_check.inc

  --echo # 2. Do DML insert/update/delete on upgraded tables
  --let $check_option = DML
  --source partition_upgrade_check.inc

  --echo # 3. Do DDL ALTER/DROP on upgraded tables
  --let $check_option = DDL
  --source partition_upgrade_check.inc

  if ($i == 0) {
    --echo
    --echo # B. Create all the tables again and validate
    --let $check_option = DROP
    --source partition_upgrade_check.inc
    --let $check_option = CREATE
    --source partition_upgrade_check.inc
    --echo
  }
  --inc $i
}

--echo
--echo # Shutdown server
--source include/shutdown_mysqld.inc

--echo
--echo # Remove upgraded data directory.
--force-rmdir $MYSQL_TMP_DIR/data_upgrade

if (!$NO_EXT_PATH)
{
  --force-rmdir $EXTERNAL_DIRECTORY

  if (!$IS_FIXED_PATH)
  {
    --force-rmdir $MYSQL_TMP_DIR/tmp
  }
}

--echo
--echo # Cleanup: Restart with default options.
--let $restart_parameters =
--source include/start_mysqld.inc
