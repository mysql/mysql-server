# WL 13352: Test Upgrade of partitioned tables from different versions
# Caller need to set OLD_VERSION: 8013, 8015, 8017, 8019
#                    OLD_LCTN: 0,1,2 [lower_case_table_names]
#                    OLD_PLATFORM: lin, win, mac [linux/windows/mac]

# To disable external file path
# --let $NO_EXT_PATH = 1

# To Enable fixed path for external files
# --let $IS_FIXED_PATH = 1

--echo # Test Upgrade from old version $OLD_VERSION

--source include/have_innodb_16k.inc
--source include/not_valgrind.inc

# For all other versions we run only on case sensitive fs
if ($OLD_VERSION != 8019) {
    --source include/have_case_sensitive_file_system.inc
    --let $OLD_LCTN = 1
}

--let $OLD_EXTN = $OLD_VERSION
--let $LCTN = --lower_case_table_names=$OLD_LCTN

# lower_case_table_names = 0 is supported only on case sensitive fs
if ($OLD_LCTN == 0) {
  --source include/have_case_sensitive_file_system.inc
}

# lower_case_table_names = 2 is supported only on case sensitive fs
if ($OLD_LCTN == 2) {
  --source include/have_case_insensitive_file_system.inc
}

# Check migration across platforms for 8.0.19
if ($OLD_VERSION == 8019)
{
  --let $OLD_EXTN = `select concat("$OLD_VERSION", "_", "$OLD_PLATFORM", "_lctn_", "$OLD_LCTN")`
  --let EXTRA_TABLES = 1
}

--echo # Stop DB server which was created by MTR default
--source include/shutdown_mysqld.inc

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
--copy_file $MYSQLTEST_VARDIR/std_data/upgrade/data_wl13352_$OLD_EXTN.zip $MYSQL_TMP_DIR/data_upgrade.zip
--file_exists $MYSQL_TMP_DIR/data_upgrade.zip
--exec unzip -qo $MYSQL_TMP_DIR/data_upgrade.zip -d $MYSQL_TMP_DIR
--remove_file $MYSQL_TMP_DIR/data_upgrade.zip

if (!$NO_EXT_PATH)
{
  --echo # Copy and unzip external directory
  --copy_file $MYSQLTEST_VARDIR/std_data/upgrade/extn_wl13352_$OLD_EXTN.zip $MYSQL_TMP_DIR/data_extn.zip
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
  --exec echo "restart: --datadir=$MYSQLD_DATADIR_UPGRADE $LCTN --log-error=$MYSQLD_LOG" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
}

if (!$NO_EXT_PATH) {
  --replace_result $MYSQLD MYSQLD $MYSQLD_DATADIR_UPGRADE MYSQLD_DATADIR_UPGRADE $MYSQLD_LOG MYSQLD_LOG $EXTERNAL_DIRECTORY EXTERNAL_DIRECTORY
  --exec echo "restart: --datadir=$MYSQLD_DATADIR_UPGRADE $LCTN --innodb_directories=$EXTERNAL_DIRECTORY --log-error=$MYSQLD_LOG" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
}

--let $wait_counter= 10000

# Additional wait for 30 minutes for valgrid run
--let add_wait_counter= 18000

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
