--disable_query_log
call mtr.add_suppression("\\[ERROR\\] .*MY-\\d+.* Can't set undo log tablespace to be encrypted.");
# See Bug #25446984: INNODB.UNDO_ENCRYPT_BOOTSTRAP HAS OCCASIONAL FAILURES ON PB2
call mtr.add_suppression("\\[ERROR\\] .*MY-\\d+.* Can't rotate encryption on undo tablespace");
--enable_query_log

let $MYSQLD_BASEDIR= `select @@basedir`;
let $START_PAGE_SIZE= `select @@innodb_page_size`;
let $LOG_SIZE= `select @@innodb_redo_log_capacity`;

# Create path for ibdata* & undo* files both DBs
--mkdir $MYSQL_TMP_DIR/innodb_undo_data_dir
--mkdir $MYSQL_TMP_DIR/innodb_data_home_dir
--mkdir $MYSQL_TMP_DIR/datadir

# Set path for --datadir
let $MYSQLD_DATADIR = $MYSQL_TMP_DIR/datadir/data;

# Set path for undo* files.
let $MYSQLD_UNDO_DATADIR = $MYSQL_TMP_DIR/innodb_undo_data_dir;

# Set path for ibdata* files.
let $MYSQLD_HOME_DATA_DIR = $MYSQL_TMP_DIR/innodb_data_home_dir;

let BOOTSTRAP_SQL=$MYSQL_TMP_DIR/boot.sql;

--echo # create bootstrap file
write_file $BOOTSTRAP_SQL;
CREATE DATABASE test;
EOF

--echo # Stop the MTR default DB server
--source include/shutdown_mysqld.inc

# Test1: bootstrap with undo encryption but no keyring, it should fail.

--source include/keyring_tests/helper/binary_backup_manifest.inc
# Set the bootstrap parameters with undo log encryption enabled
let NEW_CMD = $MYSQLD --no-defaults $PLUGIN_DIR_OPT --innodb_dedicated_server=OFF --initialize-insecure --secure-file-priv="" --innodb_redo_log_capacity=$LOG_SIZE --innodb_page_size=$START_PAGE_SIZE --innodb_data_home_dir=$MYSQLD_HOME_DATA_DIR --innodb_undo_directory=$MYSQLD_UNDO_DATADIR --basedir=$MYSQLD_BASEDIR --datadir=$MYSQLD_DATADIR --init-file=$BOOTSTRAP_SQL --innodb_undo_log_encrypt=ON </dev/null>>$MYSQLTEST_VARDIR/tmp/bootstrap.log 2>&1;

--echo # Run the bootstrap command with no keyring
--error 1,42
--exec $NEW_CMD

# Remove residue files
--force-rmdir $MYSQL_TMP_DIR/datadir
--force-rmdir $MYSQL_TMP_DIR/innodb_data_home_dir
--force-rmdir $MYSQL_TMP_DIR/innodb_undo_data_dir

# Create path for ibdata* & undo* files both DBs
--mkdir $MYSQL_TMP_DIR/innodb_undo_data_dir
--mkdir $MYSQL_TMP_DIR/innodb_data_home_dir
--mkdir $MYSQL_TMP_DIR/datadir

# Test2: bootstrap with undo encryption and with keyring, it should
# success, then restart with no undo encryption.
--source include/keyring_tests/helper/binary_restore_manifest.inc
let NEW_CMD = $MYSQLD --no-defaults $PLUGIN_DIR_OPT --innodb_dedicated_server=OFF --initialize-insecure --innodb_redo_log_capacity=$LOG_SIZE --innodb_page_size=$START_PAGE_SIZE --innodb_data_home_dir=$MYSQLD_HOME_DATA_DIR --innodb_undo_directory=$MYSQLD_UNDO_DATADIR --basedir=$MYSQLD_BASEDIR --datadir=$MYSQLD_DATADIR --init-file=$BOOTSTRAP_SQL --innodb_undo_log_encrypt=ON --secure-file-priv="" </dev/null>>$MYSQLTEST_VARDIR/tmp/bootstrap2.log 2>&1;

--echo # Run the bootstrap command with keyring
--exec $NEW_CMD

--echo # Search for particular string of encryption metadata, should success since it's encrypted.
let SEARCH_FILE= $MYSQLD_UNDO_DATADIR/undo_001;
let SEARCH_PATTERN= lCC;
--source include/search_pattern.inc

#--echo # Start the DB server with undo log encryption disabled, and no keyring. It should fail.
--source include/keyring_tests/helper/binary_backup_manifest.inc
#--let $restart_parameters="restart: $PLUGIN_DIR_OPT --innodb_page_size=$START_PAGE_SIZE --innodb_redo_log_capacity=$LOG_SIZE --innodb_data_home_dir=$MYSQLD_HOME_DATA_DIR --innodb_undo_directory=$MYSQLD_UNDO_DATADIR --datadir=$MYSQLD_DATADIR"
#--echo # start server fail!!!!
#--error 0
#--source include/start_mysqld_no_echo.inc

--echo # Start the DB server with undo log encryption disabled and keyring loaded. It should success.
--source include/keyring_tests/helper/binary_restore_manifest.inc
--let $restart_parameters="restart: $PLUGIN_DIR_OPT --innodb_data_home_dir=$MYSQLD_HOME_DATA_DIR --innodb_undo_directory=$MYSQLD_UNDO_DATADIR --datadir=$MYSQLD_DATADIR --innodb_page_size=$START_PAGE_SIZE --innodb_redo_log_capacity=$LOG_SIZE"
--source include/start_mysqld_no_echo.inc

SET GLOBAL innodb_undo_log_encrypt = ON;

CREATE TABLE tab1(c1 INT, c2 VARCHAR(30));
START TRANSACTION;
INSERT INTO tab1 VALUES (100,REPEAT('a',5)),(200,REPEAT('b',5));
SELECT * FROM tab1;
COMMIT;
SET GLOBAL innodb_undo_log_encrypt = OFF;
START TRANSACTION;
INSERT INTO tab1 VALUES (300,REPEAT('a',5)),(400,REPEAT('b',5));
COMMIT;
SELECT * FROM tab1;

# Cleanup
DROP TABLE tab1;

--source include/shutdown_mysqld.inc
# Search for particular string to confirm the encryption metadata is
# stored.
--sleep 2
let SEARCH_FILE= $MYSQLD_UNDO_DATADIR/undo_001;
let SEARCH_PATTERN= lCC;
--source include/search_pattern.inc

let SEARCH_FILE= $MYSQLD_UNDO_DATADIR/undo_002;
let SEARCH_PATTERN= lCC;
--source include/search_pattern.inc

# restart the server with MTR default
--let $restart_parameters= "restart: $PLUGIN_DIR_OPT"
--source include/start_mysqld_no_echo.inc

--remove_file $BOOTSTRAP_SQL

# Remove residue files
--remove_file $MYSQLTEST_VARDIR/tmp/bootstrap.log
--remove_file $MYSQLTEST_VARDIR/tmp/bootstrap2.log
--force-rmdir $MYSQL_TMP_DIR/datadir
--force-rmdir $MYSQL_TMP_DIR/innodb_data_home_dir
--force-rmdir $MYSQL_TMP_DIR/innodb_undo_data_dir
