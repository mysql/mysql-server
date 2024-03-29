################################################################################
# InnoDB transparent tablespace data encryption for general shared tablespace.
# This test case will test
#    - Crash during encrypting an unencrypted tablespace.
#      - And then restart without loading keyring plugin. Server should fail to
#        start.
#      - Restart with keyring pluing. Server should start and recover properly.
################################################################################

# Waiting time when (re)starting the server
--let $explicit_default_wait_counter=10000

--disable_query_log
call mtr.add_suppression("ibd can't be decrypted, please confirm the keyfile is match and keyring plugin is loaded.");
call mtr.add_suppression("Check keyring fail, please check the keyring is loaded.");
--enable_query_log

--echo #########################################################################
--echo # START : WITHOUT KEYRING PLUGIN
--echo #########################################################################
--source include/keyring_tests/helper/instance_backup_manifest.inc
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc
--echo #########
--echo # SETUP #
--echo #########

# Create an Unencrypted tablespace
CREATE TABLESPACE encrypt_ts ADD DATAFILE 'encrypt_ts.ibd' ENGINE=InnoDB ENCRYPTION="N";
# Create an unencrypted table in tablespace
CREATE TABLE t1(c1 char(100)) ENGINE=InnoDB TABLESPACE encrypt_ts;
# Insert few rows in table
--disable_query_log
INSERT INTO t1 VALUES ("SOME VALUES");
let $counter=12;
while ($counter>0)
{
  INSERT INTO test.t1 SELECT * FROM test.t1;
  dec $counter;
}
--enable_query_log

# Make sure ts file is updated with new records in table
set global innodb_buf_flush_list_now = 1;

SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME='encrypt_ts';
SELECT * FROM t1 LIMIT 10;

# Try to alter tablespace to be encrypted. Should fail as keyring is not laoded.
--error ER_CANNOT_FIND_KEY_IN_KEYRING
ALTER TABLESPACE encrypt_ts ENCRYPTION='Y';

--echo #############################################################
--echo # TEST 1 : CRASH DURING ALTER ENCRYPT A TABLESPACE.
--echo #############################################################
--echo
--echo #########################################################################
--echo # RESTART 1 : WITH KEYRING PLUGIN
--echo #########################################################################
--source include/keyring_tests/helper/instance_restore_manifest.inc
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc

--echo ############################################################
--echo # ALTER TABLESPACE 1 :    Unencrypted => Encrypted         #
--echo #                         (crash at page 10)               #
--echo ############################################################
--echo # Set Encryption process to crash at page 10
SET SESSION debug= '+d,alter_encrypt_tablespace_page_10';
SET GLOBAL innodb_log_checkpoint_now = ON;
SET GLOBAL innodb_page_cleaner_disabled_debug = 1;
SET GLOBAL innodb_dict_stats_disabled_debug = 1;
SET GLOBAL innodb_master_thread_disabled_debug = 1;

--echo # Encrypt the tablespace. It will cause crash.
--source include/expect_crash.inc
--error 0,CR_SERVER_LOST,ER_INTERNAL_ERROR
ALTER TABLESPACE encrypt_ts ENCRYPTION='Y';
--echo # Restart after crash without Keyring plugin loaded
--source include/keyring_tests/helper/instance_backup_manifest.inc
--error 1,42
--exec $MYSQLD_CMD $PLUGIN_DIR_OPT --log-error=$MYSQLTEST_VARDIR/log/my_restart.err

# Search the failure pattern in error log
let SEARCH_FILE= $MYSQLTEST_VARDIR/log/my_restart.err;
let SEARCH_PATTERN=CORRUPT LOG RECORD FOUND;
--source include/search_pattern.inc

--echo # Server shouldn't have restarted, so query should fail.
--error CR_SERVER_LOST, CR_SERVER_GONE_ERROR
SELECT * from test.t1 limit 10;

--echo #########################################################################
--echo # RESTART 2 : WITH KEYRING PLUGIN
--echo #########################################################################
--source include/keyring_tests/helper/instance_restore_manifest.inc
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/start_mysqld_no_echo.inc

--echo # Server should have restarted properly.
SELECT * from test.t1 limit 10;

--echo ###########
--echo # Cleanup #
--echo ###########
DROP TABLE t1;
DROP TABLESPACE encrypt_ts;
remove_file $SEARCH_FILE;
