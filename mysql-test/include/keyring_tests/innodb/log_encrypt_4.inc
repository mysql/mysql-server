# InnoDB transparent encryption on redo log.
# This test case will check innodb_redo_log_encrypt=ON/OFF with bootstrap
# and start server by setting innodb_redo_log_encrypt=ON/OFF

# Suppress following messages from myslqd log
--disable_query_log
call mtr.add_suppression("Error while loading keyring content. The keyring might be malformed");
call mtr.add_suppression("Resizing redo log from");
call mtr.add_suppression("Redo log has been requested to resize");
call mtr.add_suppression("Starting to delete and rewrite redo log files.");
call mtr.add_suppression("New redo log files created, LSN=");
call mtr.add_suppression("\\[ERROR\\] .* Can't set redo log files to be encrypted.");
--enable_query_log

let $old_innodb_file_per_table = `SELECT @@innodb_file_per_table`;
let $old_innodb_redo_log_encrypt = `SELECT @@innodb_redo_log_encrypt`;
let $START_PAGE_SIZE= `select @@innodb_page_size`;
let $LOG_SIZE= `select @@innodb_redo_log_capacity`;
let $LOG_SIZE_SCALED_UP= `select @@innodb_redo_log_capacity * 3 div 2`;
let $MYSQLD_BASEDIR= `select @@basedir`;

--mkdir $MYSQL_TMP_DIR/log_encrypt_dir1
--mkdir $MYSQL_TMP_DIR/log_encrypt_dir2
--mkdir $MYSQL_TMP_DIR/log_encrypt_dir3

let $MYSQLD_DATADIR1 = $MYSQL_TMP_DIR/log_encrypt_dir1;
let $MYSQLD_DATADIR2 = $MYSQL_TMP_DIR/log_encrypt_dir2;
let $MYSQLD_DATADIR3 = $MYSQL_TMP_DIR/log_encrypt_dir3;

let BOOTSTRAP_SQL=$MYSQL_TMP_DIR/boot.sql;

--echo # create bootstrap file
write_file $BOOTSTRAP_SQL;
CREATE DATABASE test;
EOF
--echo # Stop the MTR default DB server
--source include/shutdown_mysqld.inc

let NEW_CMD = $MYSQLD --no-defaults $PLUGIN_DIR_OPT --innodb_dedicated_server=OFF --initialize-insecure --innodb_redo_log_capacity=$LOG_SIZE --innodb_page_size=$START_PAGE_SIZE --basedir=$MYSQLD_BASEDIR --datadir=$MYSQLD_DATADIR1 --init-file=$BOOTSTRAP_SQL --secure-file-priv="" --innodb_redo_log_encrypt=ON </dev/null>>$MYSQLTEST_VARDIR/tmp/bootstrap1.log 2>&1;

--echo # Run the bootstrap command of datadir1
--exec $NEW_CMD

--echo # Start the DB server with datadir1
--let $restart_parameters="restart: $PLUGIN_DIR_OPT --innodb_redo_log_encrypt=ON --datadir=$MYSQLD_DATADIR1 --innodb_page_size=$START_PAGE_SIZE --innodb_redo_log_capacity=$LOG_SIZE"
--replace_regex /\.dll/.so/
--source include/start_mysqld_no_echo.inc

SELECT @@global.innodb_redo_log_encrypt ;

USE test;
CREATE TABLE tab1(c1 INT, c2 VARCHAR(30));
INSERT INTO tab1 VALUES(1, 'Test consistency undo*');
SELECT * FROM tab1;
CREATE TABLE tab2(c1 INT, c2 VARCHAR(30)) ENCRYPTION="Y";
INSERT INTO tab2 VALUES(1, 'Test consistency undo*');
SELECT * FROM tab2;
DROP TABLE tab1,tab2;

# Shutdown the server
--source include/kill_mysqld.inc

--force-rmdir $MYSQL_TMP_DIR/log_encrypt_dir1
--remove_file $MYSQLTEST_VARDIR/tmp/bootstrap1.log
--source include/keyring_tests/helper/local_keyring_file_remove.inc
#------------------------------------------------------------------------------

# Take backup of global manifest file
--source include/keyring_tests/helper/binary_backup_manifest.inc

#bootstrap should fail if keyring plugin is not loaded along with
#innodb_redo_log_encrypt=ON
let NEW_CMD = $MYSQLD --no-defaults $PLUGIN_DIR_OPT --innodb_dedicated_server=OFF --initialize-insecure --basedir=$MYSQLD_BASEDIR --datadir=$MYSQLD_DATADIR2 --init-file=$BOOTSTRAP_SQL  --secure-file-priv="" --innodb_redo_log_encrypt=ON  </dev/null>>$MYSQLTEST_VARDIR/tmp/bootstrap2.log 2>&1;

--echo # Run the bootstrap command of datadir2, it should fail since the keyring is not loaded.
--error 1,42
--exec $NEW_CMD

--force-rmdir $MYSQL_TMP_DIR/log_encrypt_dir2
--remove_file $MYSQLTEST_VARDIR/tmp/bootstrap2.log

# Restore global manifest file
--source include/keyring_tests/helper/binary_restore_manifest.inc

let NEW_CMD = $MYSQLD --no-defaults $PLUGIN_DIR_OPT --innodb_dedicated_server=OFF --initialize-insecure --innodb_redo_log_capacity=$LOG_SIZE_SCALED_UP --innodb_page_size=$START_PAGE_SIZE --basedir=$MYSQLD_BASEDIR --datadir=$MYSQLD_DATADIR2 --init-file=$BOOTSTRAP_SQL --secure-file-priv="" --innodb_redo_log_encrypt=ON </dev/null>>$MYSQLTEST_VARDIR/tmp/bootstrap2.log 2>&1;

--echo # Run the bootstrap command of datadir2
--exec $NEW_CMD

--echo # Start the DB server with datadir2
--let $restart_parameters="restart: $PLUGIN_DIR_OPT --innodb_redo_log_encrypt=ON --datadir=$MYSQLD_DATADIR2 --innodb_page_size=$START_PAGE_SIZE --innodb_redo_log_capacity=$LOG_SIZE_SCALED_UP"
--source include/start_mysqld_no_echo.inc

SELECT @@global.innodb_redo_log_encrypt ;

USE test;
CREATE TABLE tab1(c1 INT, c2 VARCHAR(30));
INSERT INTO tab1 VALUES(1, 'Test consistency undo*');
SELECT * FROM tab1;
CREATE TABLE tab2(c1 INT, c2 VARCHAR(30)) ENCRYPTION="Y";
INSERT INTO tab2 VALUES(1, 'Test consistency undo*');
SELECT * FROM tab2;
DROP TABLE tab1,tab2;

# Shutdown the server
--source include/kill_mysqld.inc

--force-rmdir $MYSQL_TMP_DIR/log_encrypt_dir2
--remove_file $MYSQLTEST_VARDIR/tmp/bootstrap2.log
--source include/keyring_tests/helper/local_keyring_file_remove.inc
#------------------------------------------------------------------------------

let NEW_CMD = $MYSQLD --no-defaults $PLUGIN_DIR_OPT --innodb_dedicated_server=OFF --initialize-insecure --innodb_redo_log_capacity=$LOG_SIZE --innodb_page_size=$START_PAGE_SIZE --basedir=$MYSQLD_BASEDIR --datadir=$MYSQLD_DATADIR3 --init-file=$BOOTSTRAP_SQL --secure-file-priv="" --innodb_redo_log_encrypt=ON </dev/null>>$MYSQLTEST_VARDIR/tmp/bootstrap3.log 2>&1;

--echo # Run the bootstrap command of datadir3
--exec $NEW_CMD

--echo # Start the DB server with datadir3 and keyring loaded.
--let $restart_parameters="restart: $PLUGIN_DIR_OPT --datadir=$MYSQLD_DATADIR3 --innodb_page_size=$START_PAGE_SIZE --innodb_redo_log_capacity=$LOG_SIZE"
--source include/start_mysqld_no_echo.inc

SELECT @@global.innodb_redo_log_encrypt ;

USE test;
CREATE TABLE tab1(c1 INT, c2 VARCHAR(30));
INSERT INTO tab1 VALUES(1, 'Test consistency undo*');
SELECT * FROM tab1;
CREATE TABLE tab2(c1 INT, c2 VARCHAR(30)) ENCRYPTION="Y";
DROP TABLE tab1;

# Shutdown the server
--source include/kill_mysqld.inc

--force-rmdir $MYSQL_TMP_DIR/log_encrypt_dir3
--remove_file $MYSQLTEST_VARDIR/tmp/bootstrap3.log
--source include/keyring_tests/helper/local_keyring_file_remove.inc
--remove_file $BOOTSTRAP_SQL

# Restart server
--let $restart_parameters= restart: $PLUGIN_DIR_OPT 
--source include/start_mysqld_no_echo.inc

# Cleanup
--disable_query_log
eval SET GLOBAL innodb_file_per_table=$old_innodb_file_per_table;
eval SET GLOBAL innodb_redo_log_encrypt=$old_innodb_redo_log_encrypt;
--enable_query_log
