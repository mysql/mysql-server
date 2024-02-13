--echo #
--echo # wl9508: Show that explicit and implicit undo tablespaces can be encrypted.
--echo #

let $MYSQLD_BASEDIR= `select @@basedir`;
let $MYSQLD_OLD_DATADIR = `select @@datadir`;

use test;
CREATE table tab1(c1 int);

--mkdir $MYSQL_TMP_DIR/log_encrypt_dir1

let $MYSQLD_DATADIR1 = $MYSQL_TMP_DIR/log_encrypt_dir1;

let BOOTSTRAP_SQL=$MYSQL_TMP_DIR/boot.sql;

--echo # create bootstrap file
write_file $BOOTSTRAP_SQL;
CREATE DATABASE test;
EOF

--echo # Stop the MTR default DB server
--source include/shutdown_mysqld.inc
--source include/keyring_tests/helper/binary_backup_manifest.inc
let NEW_CMD = $MYSQLD --no-defaults --initialize-insecure --basedir=$MYSQLD_BASEDIR --datadir=$MYSQLD_DATADIR1 --init-file=$BOOTSTRAP_SQL --secure-file-priv="" $PLUGIN_DIR_OPT </dev/null>>$MYSQLTEST_VARDIR/tmp/bootstrap1.log 2>&1;

--echo # Run the bootstrap command of datadir1
--exec $NEW_CMD

--source include/keyring_tests/helper/binary_restore_manifest.inc
--let $restart_parameters="restart: $PLUGIN_DIR_OPT --log_error_verbosity=3 --datadir=$MYSQLD_DATADIR1 --innodb_undo_log_encrypt=1 --innodb_redo_log_encrypt=1"
--source include/start_mysqld_no_echo.inc

--replace_result $MYSQLD_DATADIR1 MYSQLD_DATADIR1
SELECT @@datadir;

SELECT @@innodb_undo_log_encrypt;

CREATE UNDO TABLESPACE undo_003 ADD DATAFILE 'undo_003.ibu';
CREATE DATABASE nath;
use nath;
CREATE TABLE tab2(c1 int , c2 varchar(10)) Engine=InnoDB ENCRYPTION='Y';
INSERT INTO tab2 VALUES(2, 'VISH');
CREATE INDEX ix2 ON tab2(c2) ;

# Assertion failure: space_id == page_get_space_id(buf_block_get_frame(block))
# during wl9508 testing
CREATE UNDO TABLESPACE undo_004 ADD DATAFILE 'undo_004.ibu';

# Check ALTER and DROP with encrypt undo tablespaces
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
let $inactive_undo_space = undo_003;
source include/wait_until_undo_space_is_empty.inc;

ALTER UNDO TABLESPACE undo_004 SET INACTIVE;
let $inactive_undo_space = undo_004;
source include/wait_until_undo_space_is_empty.inc;

DELETE FROM tab2;
DROP UNDO TABLESPACE undo_003;
DROP UNDO TABLESPACE undo_004;
DROP TABLE tab2;
DROP DATABASE nath;

--echo # Stop the encrypt server
--let $shutdown_server_timeout= 300
--source include/shutdown_mysqld.inc

--source include/keyring_tests/helper/binary_backup_manifest.inc

# Restart with MTR default server
--let $wait_counter= 3000
--replace_result $MYSQLD_OLD_DATADIR OLD_DATADIR
--let $restart_parameters="restart: $PLUGIN_DIR_OPT --datadir=$MYSQLD_OLD_DATADIR"
--source include/start_mysqld_no_echo.inc

--replace_result $MYSQLD_OLD_DATADIR MYSQLD_OLD_DATADIR
SELECT @@datadir;

use test;

# clean up
DROP TABLE tab1;
--remove_file $MYSQLTEST_VARDIR/tmp/bootstrap1.log
--force-rmdir $MYSQL_TMP_DIR/log_encrypt_dir1

--echo #
--echo # bug#29006275 : ENCRYPTION MASTER KEY IS GENERATED WITH BLANK UUID
--echo #

LET $MYSQL_DATA_DIR = `select @@datadir`;

--echo # Stop the non-encrypted server
--source include/shutdown_mysqld.inc

let NEW_CMD = $MYSQLD --no-defaults --initialize-insecure --basedir=$MYSQLD_BASEDIR --datadir=$MYSQLD_DATADIR1 --init-file=$BOOTSTRAP_SQL --secure-file-priv="" $PLUGIN_DIR_OPT </dev/null>>$MYSQLTEST_VARDIR/tmp/bootstrap2.log 2>&1;

--echo # Run the bootstrap command of datadir1
--exec $NEW_CMD

--source include/keyring_tests/helper/binary_restore_manifest.inc

--let $restart_parameters="restart: $PLUGIN_DIR_OPT --log_error_verbosity=3 --datadir=$MYSQLD_DATADIR1 --innodb_undo_log_encrypt=1"
--source include/start_mysqld_no_echo.inc

--echo # Show that a master key with a blank Server UUID is not used.
SELECT * FROM performance_schema.keyring_keys where KEY_ID='INNODBKey--1';

--echo # Stop the encrypted server
--source include/shutdown_mysqld.inc

# Restart with MTR default server
--let $wait_counter= 3000
--replace_result $MYSQLD_OLD_DATADIR OLD_DATADIR
--let $restart_parameters="restart: $PLUGIN_DIR_OPT"
--source include/start_mysqld_no_echo.inc

--remove_file $MYSQL_TMP_DIR/boot.sql
--remove_file $MYSQLTEST_VARDIR/tmp/bootstrap2.log
--force-rmdir $MYSQL_TMP_DIR/log_encrypt_dir1
