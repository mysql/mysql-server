################################################################################
# InnoDB transparent tablespace data encryption for general shared tablespace.
# This test case will test, during resume operation,
#    - Test 1 : Crash just before flushing page 0 at the end of (un)encryption
#    - Test 2 : Crash just after flushing page 0 at the end of (un)encryption
#    - Test 3 : Crash just before resetting progress on page 0
#    - Test 4 : Crash just before updating tablespace flags on page 0
#    - Test 5 : Crash before starting encryption processing
#    - Test 6 : Crash after finishing encryption processing
#    - Test 7 : Crash just after inserting DDL Log Entry
################################################################################

# Waiting time when (re)starting the server
--let $explicit_default_wait_counter=10000

--disable_query_log
call mtr.add_suppression("ibd can't be decrypted, please confirm that keyring is loaded.");
call mtr.add_suppression("\\[ERROR\\] \\[[^]]*\\] \\[[^]]*\\] Encryption can't find master key, please check the keyring is loaded.");
call mtr.add_suppression("\\[ERROR\\] \\[[^]]*\\] \\[[^]]*\\] Check keyring fail, please check the keyring is loaded.");
--enable_query_log

--echo # START : WITHOUT KEYRING
--echo
--echo #########
--echo # SETUP #
--echo #########
--source include/keyring_tests/helper/instance_backup_manifest.inc
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc

let $MYSQLD_DATADIR= `select @@datadir`;

--write_file $MYSQLTEST_VARDIR/tmp/encryption_error_log.inc ERROR_LOG
# Search the pattern in error log
--let SEARCH_FILE= $MYSQLTEST_VARDIR/log/my_restart.err
--let SEARCH_PATTERN=Resuming ENCRYPTION for tablespace encrypt_ts
--source include/search_pattern.inc
--let SEARCH_PATTERN=Finished ENCRYPTION for tablespace encrypt_ts
--source include/search_pattern.inc
--remove_file $MYSQLTEST_VARDIR/log/my_restart.err
ERROR_LOG

--write_file $MYSQLTEST_VARDIR/tmp/unencryption_error_log.inc ERROR_LOG
# Search the pattern in error log
--let SEARCH_FILE= $MYSQLTEST_VARDIR/log/my_restart.err
--let SEARCH_PATTERN=Resuming DECRYPTION for tablespace encrypt_ts
--source include/search_pattern.inc
--let SEARCH_PATTERN=Finished DECRYPTION for tablespace encrypt_ts
--source include/search_pattern.inc
--remove_file $MYSQLTEST_VARDIR/log/my_restart.err
ERROR_LOG

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

--let $rc=1
--echo #-------------------------- TEST 1 -------------------------------------#

--echo # RESTART $rc : WITH KEYRING
--source include/keyring_tests/helper/instance_restore_manifest.inc
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc
--inc $rc

--let $crash_point=alter_encrypt_tablespace_crash_before_flushing_page_0
--let $crash_point_desc=just before flushing page 0 at the end
--source suite/innodb/include/resume_test_encryption.inc
--source suite/innodb/include/resume_test_unencryption.inc
--source include/keyring_tests/helper/instance_restore_manifest.inc

--echo #-------------------------- TEST 2 -------------------------------------#

--echo # RESTART $rc : WITH KEYRING
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc
--inc $rc

--let $crash_point=alter_encrypt_tablespace_crash_after_flushing_page_0
--let $crash_point_desc=just after flushing page 0 at the end
--source suite/innodb/include/resume_test_encryption.inc
--source suite/innodb/include/resume_test_unencryption.inc
--source include/keyring_tests/helper/instance_restore_manifest.inc

--echo #-------------------------- TEST 3 -------------------------------------#

--echo # RESTART $rc : WITH KEYRING
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc
--inc $rc

--let $crash_point=alter_encrypt_tablespace_crash_before_resetting_progress
--let $crash_point_desc=just before resetting_progress on page 0
--source suite/innodb/include/resume_test_encryption.inc
--source suite/innodb/include/resume_test_unencryption.inc
--source include/keyring_tests/helper/instance_restore_manifest.inc

--echo #-------------------------- TEST 4 -------------------------------------#

--echo # RESTART $rc : WITH KEYRING
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc
--inc $rc

--echo # Encrypt the tablespace.
ALTER TABLESPACE encrypt_ts ENCRYPTION='Y';

--let $crash_point=alter_encrypt_tablespace_crash_before_updating_flags
--let $crash_point_desc=just before updating flags
--source suite/innodb/include/resume_test_unencryption.inc
--source include/keyring_tests/helper/instance_restore_manifest.inc

--echo #-------------------------- TEST 5 -------------------------------------#

--echo # RESTART $rc : WITH KEYRING
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc
--inc $rc

--let $crash_point=alter_encrypt_tablespace_crash_before_processing
--let $crash_point_desc=just before encryption processing is started
--source suite/innodb/include/resume_test_encryption.inc
--source suite/innodb/include/resume_test_unencryption.inc
--source include/keyring_tests/helper/instance_restore_manifest.inc

--echo #-------------------------- TEST 6 -------------------------------------#

--echo # RESTART $rc : WITH KEYRING
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc
--inc $rc

--let $crash_point=alter_encrypt_tablespace_crash_after_processing
--let $crash_point_desc=just after encryption processing is finished
--source suite/innodb/include/resume_test_encryption.inc
--source suite/innodb/include/resume_test_unencryption.inc
--source include/keyring_tests/helper/instance_restore_manifest.inc

--echo #-------------------------- TEST 7 -------------------------------------#

--echo # RESTART $rc : WITH KEYRING
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc
--inc $rc

--let $crash_point=alter_encrypt_tablespace_crash_after_ddl_entry
--let $crash_point_desc=just after inserting DDL Log Entry
--source suite/innodb/include/resume_test_encryption.inc
--source suite/innodb/include/resume_test_unencryption.inc
--source include/keyring_tests/helper/instance_restore_manifest.inc

--echo ###########
--echo # Cleanup #
--echo ###########
DROP TABLE t1;
DROP TABLESPACE encrypt_ts;

--echo # RESTART $rc : final
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc
--remove_file $MYSQLTEST_VARDIR/tmp/encryption_error_log.inc
--remove_file $MYSQLTEST_VARDIR/tmp/unencryption_error_log.inc
