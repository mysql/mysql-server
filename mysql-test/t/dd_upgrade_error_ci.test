--source include/have_case_insensitive_file_system.inc

--let ZIP_FILE= $MYSQLTEST_VARDIR/std_data/data80011_upgrade_groupby_desc_ci_win.zip
if (`SELECT CONVERT(@@version_compile_os USING LATIN1) RLIKE '^(osx|macos)'`)
{
    --let ZIP_FILE= $MYSQLTEST_VARDIR/std_data/data80011_upgrade_groupby_desc_ci_mac.zip
}

--echo # Stop DB server which was created by MTR default
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/shutdown_mysqld.inc

--echo # ------------------------------------------------------------------
--echo # Check upgrade from 8.0 when events/triggers/views/routines contain GROUP BY DESC.
--echo # ------------------------------------------------------------------

--echo # Set different path for --datadir
let $MYSQLD_DATADIR1 = $MYSQL_TMP_DIR/data80011_upgrade_groupby_desc_ci;

--echo # DB server which was started above is not running, no need for shutdown

--echo # Copy the remote tablespace & DB zip files from suite location to working location.
--copy_file $ZIP_FILE $MYSQL_TMP_DIR/data80011_upgrade_groupby_desc_ci.zip

--echo # Check that the file exists in the working folder.
--file_exists $MYSQL_TMP_DIR/data80011_upgrade_groupby_desc_ci.zip

--echo # Unzip the zip file.
--exec unzip -qo $MYSQL_TMP_DIR/data80011_upgrade_groupby_desc_ci.zip -d $MYSQL_TMP_DIR

--echo #
--echo # Upgrade tests for WL#8693
--echo #

--echo # Starting the DB server will fail since the data dir contains
--echo # events/triggers/views/routines contain GROUP BY DESC
let MYSQLD_LOG= $MYSQL_TMP_DIR/server.log;
--error 1
--exec $MYSQLD --no-defaults $extra_args --innodb_dedicated_server=OFF --secure-file-priv="" --log-error=$MYSQLD_LOG --datadir=$MYSQLD_DATADIR1

--let SEARCH_FILE= $MYSQLD_LOG
--let SEARCH_PATTERN= Trigger 'trigger_groupby_desc' has an error in its body
--source include/search_pattern.inc

--let SEARCH_FILE= $MYSQLD_LOG
--let SEARCH_PATTERN= Error in parsing Event 'test'.'event_groupby_desc' during upgrade
--source include/search_pattern.inc

--let SEARCH_FILE= $MYSQLD_LOG
--let SEARCH_PATTERN= Error in parsing Routine 'test'.'procedure_groupby_desc' during upgrade
--source include/search_pattern.inc

--let SEARCH_FILE= $MYSQLD_LOG
--let SEARCH_PATTERN= Error in parsing View 'test'.'view_groupby_desc' during upgrade
--source include/search_pattern.inc

--let SEARCH_FILE= $MYSQLD_LOG
--let SEARCH_PATTERN= Data Dictionary initialization failed.
--source include/search_pattern.inc

--echo # Remove copied files
--remove_file $MYSQLD_LOG
--remove_file $MYSQL_TMP_DIR/data80011_upgrade_groupby_desc_ci.zip
--force-rmdir $MYSQL_TMP_DIR/data80011_upgrade_groupby_desc_ci

--echo # Restart the server with default options.
--source include/start_mysqld.inc
