--echo # WL#9289 InnoDB transparent tablespace data encryption for undo log
# This test case will test basic undo encryption support features.

--disable_query_log
call mtr.add_suppression("\\[Warning\\] .* Ignoring tablespace .* because it could not be opened");
call mtr.add_suppression("\\[ERROR\\] .* Encryption can't find master key, please check the keyring is loaded.");
call mtr.add_suppression("\\[ERROR\\] .* Failed to find tablespace for table `\.\.*`\.`\.\.*` in the cache.");
call mtr.add_suppression("\\[ERROR\\] .* Operating system error number 2 in a file operation.");
call mtr.add_suppression("\\[ERROR\\] .* The error means the system cannot find the path specified.");
call mtr.add_suppression("\\[ERROR\\] .* Could not find a valid tablespace file for");
call mtr.add_suppression("\\[ERROR\\] .* If you are installing InnoDB, remember that you must create directories yourself, InnoDB does not create them.");
call mtr.add_suppression("\\[ERROR\\] .* Can't set undo tablespace 'innodb_undo_001' to be encrypted");
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
CREATE DATABASE test DEFAULT CHARACTER SET latin1;
EOF

--echo # Stop the MTR default DB server
--source include/shutdown_mysqld.inc

--echo # Test1: bootstrap with 2 undo tablespaces and with keyring.
let NEW_CMD = $MYSQLD --no-defaults --innodb_dedicated_server=OFF --initialize-insecure --innodb_redo_log_capacity=$LOG_SIZE --innodb_page_size=$START_PAGE_SIZE --innodb_data_home_dir=$MYSQLD_HOME_DATA_DIR --innodb_undo_directory=$MYSQLD_UNDO_DATADIR --basedir=$MYSQLD_BASEDIR --datadir=$MYSQLD_DATADIR --init-file=$BOOTSTRAP_SQL --secure-file-priv="" $PLUGIN_DIR_OPT </dev/null>>$MYSQLTEST_VARDIR/tmp/bootstrap2.log 2>&1;

--echo # Run the bootstrap command with keyring
--exec $NEW_CMD

--echo # Start the DB server with undo log encryption disabled, and no keyring.
--source include/keyring_tests/helper/binary_backup_manifest.inc
--let $restart_parameters="restart: $PLUGIN_DIR_OPT --innodb_page_size=$START_PAGE_SIZE --innodb_redo_log_capacity=$LOG_SIZE --innodb_data_home_dir=$MYSQLD_HOME_DATA_DIR --innodb_undo_directory=$MYSQLD_UNDO_DATADIR --datadir=$MYSQLD_DATADIR"
--source include/start_mysqld_no_echo.inc

--echo # Enable undo log encryption, should report error in server log, since keyring is not loaded.
SET GLOBAL innodb_undo_log_encrypt = ON;

--error ER_CANNOT_FIND_KEY_IN_KEYRING
CREATE TABLE t1(c1 INT, c2 char(20)) ENCRYPTION="Y" ENGINE = InnoDB;

--echo # Start the DB server with undo log encryption disabled and keyring loaded. It should success.
--source include/keyring_tests/helper/binary_restore_manifest.inc
--let $restart_parameters="restart: $PLUGIN_DIR_OPT --innodb_data_home_dir=$MYSQLD_HOME_DATA_DIR --innodb_undo_directory=$MYSQLD_UNDO_DATADIR --datadir=$MYSQLD_DATADIR --innodb_page_size=$START_PAGE_SIZE --innodb_redo_log_capacity=$LOG_SIZE"
--source include/restart_mysqld_no_echo.inc

--echo # Enable undo log encryption, shouldn't report error in server log.
SET GLOBAL innodb_undo_log_encrypt = ON;

CREATE TABLE t1(c1 INT, c2 char(20)) ENCRYPTION='Y' ENGINE = InnoDB;

SHOW CREATE TABLE t1;

INSERT INTO t1 VALUES(0, "aaaaa");
INSERT INTO t1 VALUES(1, "bbbbb");
INSERT INTO t1 VALUES(2, "ccccc");
INSERT INTO t1 VALUES(3, "ddddd");
INSERT INTO t1 VALUES(4, "eeeee");
INSERT INTO t1 VALUES(5, "fffff");
INSERT INTO t1 VALUES(6, "ggggg");
INSERT INTO t1 VALUES(7, "hhhhh");
INSERT INTO t1 VALUES(8, "iiiii");
INSERT INTO t1 VALUES(9, "jjjjj");
INSERT INTO t1 select * from t1;
INSERT INTO t1 select * from t1;
INSERT INTO t1 select * from t1;
INSERT INTO t1 select * from t1;
INSERT INTO t1 select * from t1;
INSERT INTO t1 select * from t1;

CREATE TABLE t2(c1 INT, c2 char(20)) ENGINE = InnoDB;
INSERT INTO t2 select * from t1;

SELECT * FROM t1 ORDER BY c1 LIMIT 10;
SELECT * FROM t2 ORDER BY c1 LIMIT 10;

--echo # Start the DB server with undo log encryption enabled and keyring loaded. It should success.
--let $restart_parameters="restart: $PLUGIN_DIR_OPT --innodb_data_home_dir=$MYSQLD_HOME_DATA_DIR --innodb_undo_directory=$MYSQLD_UNDO_DATADIR --datadir=$MYSQLD_DATADIR --innodb_page_size=$START_PAGE_SIZE --innodb_redo_log_capacity=$LOG_SIZE --innodb_undo_log_encrypt=OFF"
--source include/restart_mysqld_no_echo.inc

SELECT * FROM t1 ORDER BY c1 LIMIT 10;
SELECT * FROM t2 ORDER BY c1 LIMIT 10;

DROP TABLE t1,t2;

--echo # Search for particular string of encryption metadata, should success since it's encrypted.
let SEARCH_FILE= $MYSQLD_UNDO_DATADIR/undo_001;
let SEARCH_PATTERN= lCC;
--source include/search_pattern.inc

# Crash/recovery test.
CREATE TABLE t1(c1 INT, c2 char(20)) ENGINE = InnoDB;

START TRANSACTION;
INSERT INTO t1 VALUES(0, "aaaaa");
INSERT INTO t1 VALUES(1, "bbbbb");
SET GLOBAL innodb_undo_log_encrypt = OFF;
INSERT INTO t1 VALUES(2, "ccccc");
INSERT INTO t1 VALUES(3, "ddddd");
SET GLOBAL innodb_undo_log_encrypt = ON;
INSERT INTO t1 VALUES(4, "eeeee");
INSERT INTO t1 VALUES(5, "fffff");
COMMIT;
START TRANSACTION;
INSERT INTO t1 VALUES(6, "ggggg");
SET GLOBAL innodb_undo_log_encrypt = OFF;
INSERT INTO t1 VALUES(7, "hhhhh");
INSERT INTO t1 VALUES(8, "iiiii");
SET GLOBAL innodb_undo_log_encrypt = ON;
INSERT INTO t1 VALUES(9, "jjjjj");

--echo # Kill and restart to confirm the encryption info can be retrieved properly.
--source include/kill_mysqld.inc
--exec echo "restart: $PLUGIN_DIR_OPT --innodb_data_home_dir=$MYSQLD_HOME_DATA_DIR --innodb_undo_directory=$MYSQLD_UNDO_DATADIR --datadir=$MYSQLD_DATADIR --innodb_page_size=$START_PAGE_SIZE --innodb_redo_log_capacity=$LOG_SIZE --innodb_undo_log_encrypt=ON" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect

--enable_reconnect
--source include/wait_until_connected_again.inc
--disable_reconnect

SELECT * FROM t1 ORDER BY c1 LIMIT 10;
DELETE FROM t1;

START TRANSACTION;
INSERT INTO t1 VALUES(0, "aaaaa");
INSERT INTO t1 VALUES(1, "bbbbb");
INSERT INTO t1 VALUES(2, "ccccc");
INSERT INTO t1 VALUES(3, "ddddd");
INSERT INTO t1 VALUES(4, "eeeee");
INSERT INTO t1 VALUES(5, "fffff");
INSERT INTO t1 VALUES(6, "ggggg");
INSERT INTO t1 VALUES(7, "hhhhh");
INSERT INTO t1 VALUES(8, "iiiii");
INSERT INTO t1 VALUES(9, "jjjjj");

--echo # Start the DB server with undo log encryption disabled and keyring loaded. It should succeed.
--let $restart_parameters="restart: $PLUGIN_DIR_OPT --innodb_data_home_dir=$MYSQLD_HOME_DATA_DIR --innodb_undo_directory=$MYSQLD_UNDO_DATADIR --datadir=$MYSQLD_DATADIR --innodb_page_size=$START_PAGE_SIZE --innodb_redo_log_capacity=$LOG_SIZE"
--source include/restart_mysqld_no_echo.inc

SELECT * FROM t1 ORDER BY c1 LIMIT 10;

# Test key rotation.
ALTER INSTANCE ROTATE INNODB MASTER KEY;

INSERT INTO t1 VALUES(0, "aaaaa");
INSERT INTO t1 VALUES(1, "bbbbb");
INSERT INTO t1 VALUES(2, "ccccc");
INSERT INTO t1 VALUES(3, "ddddd");
INSERT INTO t1 VALUES(4, "eeeee");
INSERT INTO t1 VALUES(5, "fffff");
INSERT INTO t1 VALUES(6, "ggggg");
INSERT INTO t1 VALUES(7, "hhhhh");
INSERT INTO t1 VALUES(8, "iiiii");
INSERT INTO t1 VALUES(9, "jjjjj");

FLUSH ENGINE LOGS;

--echo # Start the DB server with undo log encryption and keyring loaded. It should success.
--let $restart_parameters="restart: $PLUGIN_DIR_OPT --innodb_data_home_dir=$MYSQLD_HOME_DATA_DIR --innodb_undo_directory=$MYSQLD_UNDO_DATADIR --datadir=$MYSQLD_DATADIR --innodb_page_size=$START_PAGE_SIZE --innodb_redo_log_capacity=$LOG_SIZE --innodb_undo_log_encrypt=ON"
--replace_result $MYSQL_TMP_DIR TMP_DIR $KEYRING_PLUGIN_OPT --plugin-dir=KEYRING_PATH $MYSQLD_HOME_DATA_DIR HOME_DIR $MYSQLD_UNDO_DATADIR UNDO_DIR $MYSQLD_DATADIR DATADIR $START_PAGE_SIZE PAGE_SIZE $LOG_SIZE LOG_SIZE
--replace_regex /\.dll/.so/
--source include/restart_mysqld_no_echo.inc

SELECT * FROM t1 ORDER BY c1 LIMIT 10;

--echo # Cleanup
DROP TABLE t1;

--echo # restart the server with MTR default
--let $restart_parameters="restart: $PLUGIN_DIR_OPT"
--source include/restart_mysqld_no_echo.inc

--remove_file $BOOTSTRAP_SQL

--echo # Remove residue files
--force-rmdir $MYSQL_TMP_DIR/datadir
--force-rmdir $MYSQL_TMP_DIR/innodb_data_home_dir
--force-rmdir $MYSQL_TMP_DIR/innodb_undo_data_dir
--remove_file $MYSQLTEST_VARDIR/tmp/bootstrap2.log
