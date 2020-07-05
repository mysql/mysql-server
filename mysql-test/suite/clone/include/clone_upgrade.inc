## Upgrade 5.7 database server. Caller must set MYSQLD_DATADIR_UPGRADE and
## remove the directory after shutting down the upgraded database.

--echo # Stop DB server which was created by MTR default
--source include/shutdown_mysqld.inc

--echo # Copy and unzip old version data directory.
--copy_file $MYSQLTEST_VARDIR/std_data/upgrade/data_57_clone_lin.zip $MYSQL_TMP_DIR/data_upgrade.zip
--file_exists $MYSQL_TMP_DIR/data_upgrade.zip
--exec unzip -qo $MYSQL_TMP_DIR/data_upgrade.zip -d $MYSQL_TMP_DIR
--remove_file $MYSQL_TMP_DIR/data_upgrade.zip

--echo
--echo # Upgrade to latest mysql version
let $MYSQLD_LOG= $MYSQLTEST_VARDIR/log/mysqld_upgrade.log;

--replace_result $MYSQLD MYSQLD $MYSQLD_DATADIR_UPGRADE MYSQLD_DATADIR_UPGRADE $MYSQLD_LOG MYSQLD_LOG
--exec echo "restart: --datadir=$MYSQLD_DATADIR_UPGRADE --log-error=$MYSQLD_LOG" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect

--let $wait_counter= 10000

# Additional wait for 30 minutes for valgrid run
--let add_wait_counter= 18000

--enable_reconnect
--source include/wait_until_connected_again.inc

