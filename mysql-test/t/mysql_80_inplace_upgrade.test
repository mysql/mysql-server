--source include/big_test.inc

--let $lctn= query_get_value(SHOW VARIABLES LIKE 'lower_case_table_names', Value, 1)

--let $MYSQLD_DATADIR1= $MYSQL_TMP_DIR/data_80011
--let $ZIP_FILE= $MYSQLTEST_VARDIR/std_data/upgrade/data_80011.zip
if ($lctn == 1)
{
  --let $MYSQLD_DATADIR1= $MYSQL_TMP_DIR/data_80011_ci
  --let $ZIP_FILE= $MYSQLTEST_VARDIR/std_data/upgrade/data_80011_ci_win.zip
}
if ($lctn == 2)
{
  --let $MYSQLD_DATADIR1= $MYSQL_TMP_DIR/data_80011_ci
  --let $ZIP_FILE= $MYSQLTEST_VARDIR/std_data/upgrade/data_80011_ci_mac.zip
}

--source include/not_valgrind.inc
--source include/have_debug.inc
--source include/have_innodb_16k.inc
--source include/mysql_upgrade_preparation.inc

--let $MYSQLD_DATADIR= `select @@datadir`

--echo ###########################################################################
--echo # Stop the default mtr server
--echo ###########################################################################

--echo # Stop DB server which was created by MTR default
--let $shutdown_server_timeout = 300
--source include/shutdown_mysqld.inc

--echo ###########################################################################
--echo # Setup the 8.0.11 data directory
--echo ###########################################################################

--echo # Copy the remote tablespace & DB zip files from suite location to working location.
--copy_file $ZIP_FILE $MYSQL_TMP_DIR/data_80011.zip

--echo # Check that the file exists in the working folder.
--file_exists $MYSQL_TMP_DIR/data_80011.zip

--echo # Unzip the zip file.
--exec unzip -qo $MYSQL_TMP_DIR/data_80011.zip -d $MYSQL_TMP_DIR

--echo ###########################################################################
--echo # Test the --upgrade=NONE option with a 8.0.11 data directory
--echo ###########################################################################

--let $MYSQLD_LOG= $MYSQLTEST_VARDIR/log/mysql80011_no_upgrade.log
--replace_result $MYSQLD MYSQLD $MYSQLD_DATADIR1 MYSQLD_DATADIR1 $MYSQLD_LOG MYSQLD_LOG
--error 1
--exec $MYSQLD --no-defaults --secure-file-priv="" --datadir=$MYSQLD_DATADIR1 --upgrade=NONE --log-error=$MYSQLD_LOG

--let SEARCH_FILE= $MYSQLD_LOG
--echo # Search for the error message in the server error log.
--let SEARCH_PATTERN= Server shutting down because upgrade is required, yet prohibited by the command line option \'--upgrade=NONE\'\.
--source include/search_pattern.inc

--echo ###########################################################################
--echo # Test the --upgrade=MINIMAL option with a 8.0.11 data directory
--echo ###########################################################################

--let $MYSQLD_LOG= $MYSQLTEST_VARDIR/log/mysql80011_skip_upgrade.log
--replace_result $MYSQLD MYSQLD $MYSQLD_DATADIR1 MYSQLD_DATADIR1 $MYSQLD_LOG MYSQLD_LOG
--exec echo "restart: --datadir=$MYSQLD_DATADIR1 --upgrade=MINIMAL --skip-grant-tables --log-error=$MYSQLD_LOG" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--let $wait_counter= 10000
--source include/wait_until_connected_again.inc

--let $shutdown_server_timeout = 300
--source include/shutdown_mysqld.inc

--let SEARCH_FILE= $MYSQLD_LOG
--echo # Search for the error message in the server error log.
--let SEARCH_PATTERN= Server upgrade is required, but skipped by command line option \'--upgrade=MINIMAL\'\.
--source include/search_pattern.inc

--echo ###########################################################################
--echo # Test the --upgrade=NONE option with a 8.0.11 data directory with upgraded
--echo # data dictionary but skipped server upgrade
--echo ###########################################################################

--let $MYSQLD_LOG= $MYSQLTEST_VARDIR/log/mysql80011_no_upgrade_after_skip.log
--replace_result $MYSQLD MYSQLD $MYSQLD_DATADIR1 MYSQLD_DATADIR1 $MYSQLD_LOG MYSQLD_LOG
--error 1
--exec $MYSQLD --no-defaults --secure-file-priv="" --datadir=$MYSQLD_DATADIR1 --upgrade=NONE --log-error=$MYSQLD_LOG

--let SEARCH_FILE= $MYSQLD_LOG
--echo # Search for the error message in the server error log.
--let SEARCH_PATTERN= Server shutting down because upgrade is required, yet prohibited by the command line option \'--upgrade=NONE\'\.
--source include/search_pattern.inc

--echo ###########################################################################
--echo # Complete the upgrade on a data directory that has an upgraded data
--echo # dictionary but skipped server upgrade
--echo ###########################################################################

--let $MYSQLD_LOG= $MYSQLTEST_VARDIR/log/mysql80011_upgrade_after_skip.log
--replace_result $MYSQLD MYSQLD $MYSQLD_DATADIR1 MYSQLD_DATADIR1 $MYSQLD_LOG MYSQLD_LOG
--exec echo "restart: --datadir=$MYSQLD_DATADIR1 --log-error=$MYSQLD_LOG" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--let $wait_counter= 10000
--source include/wait_until_connected_again.inc

--echo # There should be no errors
--let SEARCH_FILE= $MYSQLD_LOG
--let SEARCH_PATTERN= \[ERROR\]
--source include/search_pattern.inc

# It should have created a file in the MySQL Servers datadir
--let $MYSQLD_DATADIR= `select @@datadir`
--file_exists $MYSQLD_DATADIR/mysql_upgrade_history

--echo ###########################################################################
--echo # Test upgrade of help tables
--echo ###########################################################################

--echo # Truncate a help table
TRUNCATE TABLE mysql.help_topic;
SELECT COUNT(*) = 0 FROM mysql.help_topic;

--let $MYSQLD_LOG= $MYSQLTEST_VARDIR/log/mysql80_upgrade_help_tables.log
--replace_result $MYSQLD_LOG MYSQLD_LOG
--let $restart_parameters = restart: --upgrade=FORCE --log-error=$MYSQLD_LOG
--let $wait_counter=10000
--source include/restart_mysqld.inc

SELECT COUNT(*) != 0 FROM mysql.help_topic;

--echo ###########################################################################
--echo # Cleanup
--echo ###########################################################################

--let $shutdown_server_timeout = 300
--source include/shutdown_mysqld.inc

--force-rmdir $MYSQLD_DATADIR1
--remove_file $MYSQL_TMP_DIR/data_80011.zip

--echo ###########################################################################
--echo # Setup 8.0.11 data directory
--echo ###########################################################################

--echo # Copy the remote tablespace & DB zip files from suite location to working location.
--copy_file $ZIP_FILE $MYSQL_TMP_DIR/data_80011.zip

--echo # Check that the file exists in the working folder.
--file_exists $MYSQL_TMP_DIR/data_80011.zip

--echo # Unzip the zip file.
--exec unzip -qo $MYSQL_TMP_DIR/data_80011.zip -d $MYSQL_TMP_DIR

--echo ###########################################################################
--echo # Test complete upgrade on 8.0.11 data directory
--echo ###########################################################################

--let $MYSQLD_LOG= $MYSQLTEST_VARDIR/log/mysql80011_upgrade_complete.log
--replace_result $MYSQLD MYSQLD $MYSQLD_DATADIR1 MYSQLD_DATADIR1 $MYSQLD_LOG MYSQLD_LOG
--exec echo "restart: --datadir=$MYSQLD_DATADIR1 --log-error=$MYSQLD_LOG" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--let $wait_counter= 10000
--source include/wait_until_connected_again.inc

--echo # There should be no errors
--let SEARCH_FILE= $MYSQLD_LOG
--let SEARCH_PATTERN= \[ERROR\]
--source include/search_pattern.inc

# It should have created a file in the MySQL Servers datadir
--let $MYSQLD_DATADIR= `select @@datadir`
--file_exists $MYSQLD_DATADIR/mysql_upgrade_history

--echo # Stop DB server
--let $shutdown_server_timeout = 300
--source include/shutdown_mysqld.inc

--echo ###########################################################################
--echo # Cleanup
--echo ###########################################################################

--force-rmdir $MYSQLD_DATADIR1
--remove_file $MYSQL_TMP_DIR/data_80011.zip

--let $restart_parameters = restart:
--source include/start_mysqld.inc
--source include/mysql_upgrade_cleanup.inc
