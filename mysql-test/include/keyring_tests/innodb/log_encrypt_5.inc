# InnoDB transparent encryption on redo log.
# This test case will test basic encryption support features.

#Suppress following messages from myslqd log
--disable_query_log
call mtr.add_suppression("Error while loading keyring content. The keyring might be malformed");
call mtr.add_suppression("\\[Warning\\] .* Ignoring tablespace .* because it could not be opened");
call mtr.add_suppression("\\[ERROR\\] .* Failed to find tablespace for table `\.\.*`\.`\.\.*` in the cache.");
call mtr.add_suppression("\\[ERROR\\] .* Can't set redo log files to be encrypted.");
--enable_query_log

let $old_innodb_file_per_table = `SELECT @@innodb_file_per_table`;
let $old_innodb_redo_log_encrypt = `SELECT @@innodb_redo_log_encrypt`;
let $START_PAGE_SIZE= `select @@innodb_page_size`;
let $LOG_SIZE= `select @@innodb_redo_log_capacity`;

let MYSQLD_DATADIR =`SELECT @@datadir`;
let $MYSQLD_BASEDIR= `select @@basedir`;
--mkdir $MYSQL_TMP_DIR/log_encrypt_dir1
--mkdir $MYSQL_TMP_DIR/innodb_undo_data_dir
let $MYSQLD_DATADIR1 = $MYSQL_TMP_DIR/log_encrypt_dir1;
# Set path for undo* files.
let $MYSQLD_UNDO_DATADIR = $MYSQL_TMP_DIR/innodb_undo_data_dir;

let BOOTSTRAP_SQL=$MYSQL_TMP_DIR/boot.sql;

--echo # create bootstrap file
write_file $BOOTSTRAP_SQL;
CREATE DATABASE test;
EOF
--echo # Stop the MTR default DB server
--source include/shutdown_mysqld.inc
let NEW_CMD = $MYSQLD --no-defaults --innodb_dedicated_server=OFF --initialize-insecure --innodb_redo_log_capacity=$LOG_SIZE --innodb_page_size=$START_PAGE_SIZE --basedir=$MYSQLD_BASEDIR --datadir=$MYSQLD_DATADIR1 --init-file=$BOOTSTRAP_SQL --innodb_undo_directory=$MYSQLD_UNDO_DATADIR --secure-file-priv="" $PLUGIN_DIR_OPT </dev/null>>$MYSQLTEST_VARDIR/tmp/bootstrap1.log 2>&1;

--echo # Run the bootstrap command of datadir1
--exec $NEW_CMD

--let $restart_parameters= restart: $PLUGIN_DIR_OPT --innodb_redo_log_capacity=$LOG_SIZE --innodb_page_size=$START_PAGE_SIZE --datadir=$MYSQLD_DATADIR1 --innodb_undo_directory=$MYSQLD_UNDO_DATADIR
--source include/start_mysqld_no_echo.inc

--disable_warnings
DROP DATABASE IF EXISTS tde_db;
CREATE DATABASE tde_db;
USE tde_db;
--enable_warnings

# Set innodb_redo_log_encrypt = ON/OFF
# Expected result : Redo file encryption is enabled/disable for redo log.
SET GLOBAL innodb_redo_log_encrypt = 1;
SET GLOBAL innodb_undo_log_encrypt = 1;
SELECT @@global.innodb_redo_log_encrypt ;
CREATE TABLE tde_db.t1 (a BIGINT PRIMARY KEY, b LONGBLOB) ENGINE=InnoDB;
INSERT INTO t1 (a, b) VALUES (1, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t1;
CREATE TABLE tde_db.t2 (a BIGINT PRIMARY KEY, b LONGBLOB)
                        ENCRYPTION='Y' ENGINE=InnoDB;
INSERT INTO t2 (a, b) VALUES (1, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t2;
SET GLOBAL innodb_redo_log_encrypt = 0;
SET GLOBAL innodb_undo_log_encrypt = 0;
SELECT @@global.innodb_redo_log_encrypt ;
CREATE TABLE tde_db.t3 (a BIGINT PRIMARY KEY, b LONGBLOB) ENGINE=InnoDB;
INSERT INTO t3 (a, b) VALUES (1, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t3;
CREATE TABLE tde_db.t4 (a BIGINT PRIMARY KEY, b LONGBLOB)
                        ENCRYPTION='Y' ENGINE=InnoDB;
INSERT INTO t4 (a, b) VALUES (1, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t4;

SET GLOBAL innodb_redo_log_encrypt = 1;
SET GLOBAL innodb_undo_log_encrypt = 1;
SELECT @@global.innodb_redo_log_encrypt ;
CREATE TABLE tde_db.t5 (a BIGINT PRIMARY KEY, b LONGBLOB) ENGINE=InnoDB;
INSERT INTO t5 (a, b) VALUES (1, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t5;
CREATE TABLE tde_db.t6 (a BIGINT PRIMARY KEY, b LONGBLOB)
                        ENCRYPTION='Y' ENGINE=InnoDB;
INSERT INTO t6 (a, b) VALUES (1, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t6;
START TRANSACTION;
INSERT INTO t5 (a, b) VALUES (2, REPEAT('a', 6*512*512));
INSERT INTO t6 (a, b) VALUES (2, REPEAT('a', 6*512*512));
COMMIT;
START TRANSACTION;
INSERT INTO t5 (a, b) VALUES (3, REPEAT('a', 6*512*512));
INSERT INTO t6 (a, b) VALUES (3, REPEAT('a', 6*512*512));
ROLLBACK;
SELECT a,LEFT(b,10) FROM tde_db.t5;
SELECT a,LEFT(b,10) FROM tde_db.t6;

DROP TABLE tde_db.t1,tde_db.t2,tde_db.t3,tde_db.t4,tde_db.t5,tde_db.t6;
DROP DATABASE tde_db;
--let $restart_parameters= restart:
--source include/restart_mysqld_no_echo.inc

# Cleanup
--force-rmdir $MYSQL_TMP_DIR/log_encrypt_dir1
--force-rmdir $MYSQL_TMP_DIR/innodb_undo_data_dir
--remove_file $MYSQLTEST_VARDIR/tmp/bootstrap1.log
--remove_file $BOOTSTRAP_SQL
--disable_query_log
eval SET GLOBAL innodb_file_per_table=$old_innodb_file_per_table;
eval SET GLOBAL innodb_redo_log_encrypt=$old_innodb_redo_log_encrypt;
--enable_query_log
