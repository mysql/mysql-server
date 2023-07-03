# Test tries restart after redo log encryption is enabled
# - Error   :restart without keyring 
# - Pass    :restart without keyring and --innodb_force_recovery=6
# - Pass    :remove redo files,restart without keyring

--source include/no_valgrind_without_big.inc

--disable_query_log
#Suppress following messages from myslqd log
call mtr.add_suppression("Error while loading keyring content. The keyring might be malformed");
call mtr.add_suppression("\\[Error\\] .*MY-\\d+.* Encryption can't find masterkey, please check the keyring is loaded.");
call mtr.add_suppression("\\[ERROR\\] .*MY-\\d+.* Failed to find tablespace for table `\.\.*`\.`\.\.*` in the cache.");
call mtr.add_suppression("\\[ERROR\\] .*MY-\\d+.* If you are installing InnoDB, remember that you must create directories yourself, InnoDB does not create them.");
call mtr.add_suppression (" New redo log files created");
--enable_query_log

let $old_innodb_file_per_table = `SELECT @@innodb_file_per_table`;
let $old_innodb_redo_log_encrypt = `SELECT @@innodb_redo_log_encrypt`;
let $START_PAGE_SIZE= `select @@innodb_page_size`;
let $LOG_SIZE= `select @@innodb_redo_log_capacity`;
let $MYSQLD_BASEDIR= `select @@basedir`;

--mkdir $MYSQL_TMP_DIR/log_encrypt_dir1

let $MYSQLD_DATADIR1 = $MYSQL_TMP_DIR/log_encrypt_dir1;

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

--disable_warnings
DROP DATABASE IF EXISTS tde_db;
CREATE DATABASE tde_db;
USE tde_db;
--enable_warnings

# Checking keyring status and Set innodb_redo_log_encrypt = ON/OFF
# Expected result : Keyring component should be active;
# Redo file encryption is enabled/disable for redo log.

SELECT status_key, status_value FROM
performance_schema.keyring_component_status
WHERE status_key LIKE '%name%' OR status_key LIKE '%status%';

SET GLOBAL innodb_redo_log_encrypt = 1;
SELECT @@global.innodb_redo_log_encrypt ;

SELECT @@global.innodb_redo_log_encrypt ;
CREATE TABLE tde_db.t1 (a BIGINT PRIMARY KEY, b LONGBLOB) ENGINE=InnoDB;
INSERT INTO t1 (a, b) VALUES (1, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t1;
CREATE TABLE tde_db.t2 (a BIGINT PRIMARY KEY, b LONGBLOB)
                        ENCRYPTION='Y' ENGINE=InnoDB;
INSERT INTO t2 (a, b) VALUES (1, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t2;
SELECT @@global.innodb_redo_log_encrypt ;
CREATE TABLE tde_db.t3 (a BIGINT PRIMARY KEY, b LONGBLOB) ENGINE=InnoDB;
INSERT INTO t3 (a, b) VALUES (1, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t3;
CREATE TABLE tde_db.t4 (a BIGINT PRIMARY KEY, b LONGBLOB)
                        ENCRYPTION='Y' ENGINE=InnoDB;
INSERT INTO t4 (a, b) VALUES (1, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t4;

SELECT @@global.innodb_redo_log_encrypt ;
CREATE TABLE tde_db.t5 (a BIGINT PRIMARY KEY, b LONGBLOB) ENGINE=InnoDB;
INSERT INTO t5 (a, b) VALUES (1, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t5;
CREATE TABLE tde_db.t6 (a BIGINT PRIMARY KEY, b LONGBLOB)
                        ENCRYPTION='Y' ENGINE=InnoDB;
INSERT INTO t6 (a, b) VALUES (1, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t6;

DROP DATABASE tde_db;

# shutdown server
--let $_server_id= `SELECT @@server_id`
--source include/shutdown_mysqld.inc

# Take backup of global manifest file
--source include/keyring_tests/helper/binary_backup_manifest.inc

--echo # Try starting without keyring : Error
let NEW_CMD = $MYSQLD --no-defaults $PLUGIN_DIR_OPT --innodb_dedicated_server=OFF --innodb_page_size=$START_PAGE_SIZE --innodb_redo_log_capacity=$LOG_SIZE --basedir=$MYSQLD_BASEDIR --datadir=$MYSQLD_DATADIR1  --secure-file-priv="" --console </dev/null>>$MYSQL_TMP_DIR/wl9290.err 2>&1;
--error 1,42
--exec $NEW_CMD

--echo # Search for error message
let SEARCH_FILE= $MYSQL_TMP_DIR/wl9290.err;
let SEARCH_PATTERN=  Redo log was encrypted, but keyring is not loaded;
--source include/search_pattern.inc
let SEARCH_PATTERN= Aborting;
--source include/search_pattern.inc

--echo # Restart without keyring plugin possible if redo files removed
--remove_files_wildcard $MYSQLD_DATADIR1/#innodb_redo #ib_redo*

--echo # Start the DB server with datadir1
--let $restart_parameters="restart: $PLUGIN_DIR_OPT --datadir=$MYSQLD_DATADIR1 --innodb_page_size=$START_PAGE_SIZE --innodb_redo_log_capacity=$LOG_SIZE"
--replace_regex /\.dll/.so/
--source include/start_mysqld_no_echo.inc

# shutdown server
--let $_server_id= `SELECT @@server_id`
--source include/shutdown_mysqld.inc

--echo # Try starting without keyring and --innodb_force_recovery=SRV_FORCE_NO_LOG_REDO. 
--echo # Start the DB server with datadir1
--let $restart_parameters="restart: $PLUGIN_DIR_OPT --datadir=$MYSQLD_DATADIR1 --innodb_page_size=$START_PAGE_SIZE --innodb_redo_log_capacity=$LOG_SIZE --innodb_force_recovery=6 --no-console --log-error=$MYSQL_TMP_DIR/wl9290_2.err "
--replace_regex /\.dll/.so/
--source include/start_mysqld_no_echo.inc
SELECT 1;

# shutdown server
--let $_server_id= `SELECT @@server_id`
--source include/shutdown_mysqld.inc

# Restore global manifest file
--source include/keyring_tests/helper/binary_restore_manifest.inc

# restart the server with MTR default
--let $restart_parameters="restart: $PLUGIN_DIR_OPT"
--source include/start_mysqld_no_echo.inc

--remove_file $BOOTSTRAP_SQL
--remove_file $MYSQL_TMP_DIR/wl9290.err
--remove_file $MYSQL_TMP_DIR/wl9290_2.err

# Remove residue files
--force-rmdir $MYSQLD_DATADIR1
--remove_file $MYSQLTEST_VARDIR/tmp/bootstrap1.log
