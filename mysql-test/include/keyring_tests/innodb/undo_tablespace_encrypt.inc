################################################################################
# Test script to test UNDO tablespace encryption setting to be displayed in
# INFORMATION_SCHEMA.INNODB_TABLESPACES.
################################################################################

# Note:
#  Due to encrypt options used below its not possible to return to clean
#  state: create and use separate datadir for this test

--echo #
--echo # Bug #29492911 : ENABLING UNDO-TABLESPACE ENCRYPTION DOESN'T MARK TABLESPACE ENCRYPTION FLAG
--echo #

let $MYSQL_DATA_DIR = `select @@datadir`;

SHOW VARIABLES LIKE "%innodb_undo_log_encrypt%";

--let $INNODB_FILE_PER_TABLE= `SELECT @@innodb_file_per_table`
--let $PAGE_SIZE= `select @@innodb_page_size`
--let $LOG_SIZE= `select @@innodb_redo_log_capacity`


--let $MYSQL_ENC_DATADIR= $MYSQL_TMP_DIR/undo_tablespace_encrypt_dir
--mkdir $MYSQL_ENC_DATADIR

--let $BOOTSTRAP_LOG= $MYSQL_TMP_DIR/bootstrap_enc.log

--let $BOOTSTRAP_SQL= $MYSQL_TMP_DIR/boot.sql
--write_file $BOOTSTRAP_SQL
CREATE DATABASE test;
EOF

--source include/shutdown_mysqld.inc
--source include/wait_until_disconnected.inc

--source include/keyring_tests/helper/binary_backup_manifest.inc
--let NEW_CMD = $MYSQLD --no-defaults --innodb_dedicated_server=OFF --initialize-insecure --innodb_redo_log_capacity=$LOG_SIZE --innodb_page_size=$PAGE_SIZE --datadir=$MYSQL_ENC_DATADIR --init-file=$BOOTSTRAP_SQL --secure-file-priv="" >>$BOOTSTRAP_LOG 2>&1

--exec $NEW_CMD

--echo ###############################################################################
--echo # With keyring and innodb_undo_log_encrypt=ON
--echo ###############################################################################
--source include/keyring_tests/helper/binary_restore_manifest.inc
--let $restart_parameters="restart: $PLUGIN_DIR_OPT --datadir=$MYSQL_ENC_DATADIR --log_error_verbosity=3 --innodb_undo_log_encrypt=ON --log-error=$MYSQLTEST_VARDIR/log/restart_1.err "
--source include/start_mysqld_no_echo.inc

SHOW VARIABLES LIKE "%innodb_undo_log_encrypt%";

--echo # Create a new UNDO tablespace. Should be created as ENCRYPTED.
CREATE UNDO TABLESPACE undo_3 ADD DATAFILE 'undo_3.ibu';
SELECT NAME, SPACE_TYPE, ENCRYPTION, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE SPACE_TYPE='Undo';

SET GLOBAL innodb_undo_log_encrypt=OFF;
SHOW VARIABLES LIKE "%innodb_undo_log_encrypt%";
SELECT NAME, SPACE_TYPE, ENCRYPTION, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE SPACE_TYPE='Undo';

--echo # Create a new UNDO tablespace. Should be created as UNENCRYPTED.
CREATE UNDO TABLESPACE undo_4 ADD DATAFILE 'undo_4.ibu';
SELECT NAME, SPACE_TYPE, ENCRYPTION, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE SPACE_TYPE='Undo';

SET GLOBAL innodb_undo_log_encrypt=ON;
SHOW VARIABLES LIKE "%innodb_undo_log_encrypt%";
SELECT NAME, SPACE_TYPE, ENCRYPTION, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE SPACE_TYPE='Undo';

--echo ###############################################################################
--echo # With keyring and innodb_undo_log_encrypt=OFF
--echo ###############################################################################
--let $restart_parameters="restart: $PLUGIN_DIR_OPT --datadir=$MYSQL_ENC_DATADIR --log_error_verbosity=3 --innodb_undo_log_encrypt=OFF --log-error=$MYSQLTEST_VARDIR/log/restart_1.err"
--source include/restart_mysqld_no_echo.inc

# Mark UNDO tablespace inactive so that it could be dropped.
ALTER UNDO TABLESPACE undo_3 set INACTIVE;
SELECT NAME, SPACE_TYPE, ENCRYPTION, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE SPACE_TYPE='Undo';
let $inactive_undo_space = undo_3;
source include/wait_until_undo_space_is_empty.inc;
DROP UNDO TABLESPACE undo_3;

# Mark UNDO tablespace inactive so that it could be dropped.
ALTER UNDO TABLESPACE undo_4 set INACTIVE;
SELECT NAME, SPACE_TYPE, ENCRYPTION, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE SPACE_TYPE='Undo';
let $inactive_undo_space = undo_4;
source include/wait_until_undo_space_is_empty.inc;
DROP UNDO TABLESPACE undo_4;

SELECT NAME, SPACE_TYPE, ENCRYPTION, STATE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE SPACE_TYPE='Undo';

# Reset to initial state
--source include/shutdown_mysqld.inc
--source include/wait_until_disconnected.inc
--let $restart_parameters="restart: $PLUGIN_DIR_OPT"
--source include/start_mysqld_no_echo.inc

--remove_file $BOOTSTRAP_LOG
--remove_file $BOOTSTRAP_SQL
--force-rmdir $MYSQL_ENC_DATADIR
