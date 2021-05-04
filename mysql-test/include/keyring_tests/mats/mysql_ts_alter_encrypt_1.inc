################################################################################
# InnoDB transparent tablespace data encryption for mysql tablespace.
# For mysql tablespace, this test will test
#    1 - Normal alter encryption
#        - encryption='y' to encryption='n'
#        - encryption='n' to encryption='y'
#    2 - Crash during altering mysql tablespace encryption
#        - encryption='y' to encryption='n'
#        - encryption='n' to encryption='y'
#    3 - Crash
#        - just before encryption processing starts
#        - just after encryption processing finishes
#    4 - Crash during master key rotation
#    5 - Privilege check
################################################################################

--disable_query_log
call mtr.add_suppression("Error generating data for Data ID:");
call mtr.add_suppression("Encryption key missing:");
call mtr.add_suppression("Decrypting a page in doublewrite file failed:");
--enable_query_log

# Waiting time when (re)starting the server
--let $explict_default_wait_counter=10000

--echo
--echo #############################################################
--echo # TEST 1 : NORMAL ALTER ENCRYPT mysql TABLESPACE.
--echo #############################################################
--echo
--echo #########################################################################
--echo # RESTART 1 : WITH KEYRING
--echo #########################################################################
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc
SET debug='+d,skip_dd_table_access_check';

--echo # Initially, mysql should be unencrypted by default
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME='mysql';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE NAME='mysql';

ALTER TABLESPACE mysql ENCRYPTION='Y';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME='mysql';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE NAME='mysql';

ALTER TABLESPACE mysql ENCRYPTION='N';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME='mysql';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE NAME='mysql';

--echo
--echo #############################################################
--echo # TEST 2 : CRASH DURING ALTER ENCRYPT mysql TABLESPACE.
--echo #############################################################
--echo
--echo ############################################################
--echo # ALTER TABLESPACE 1 :    Unencrypted => Encrypted         #
--echo #                         (crash at page 10)               #
--echo ############################################################
--echo # Set Encryption process to crash at page 10
SET SESSION debug= '+d,alter_encrypt_tablespace_page_10';

--echo # Encrypt the tablespace. It will cause crash.
--source include/expect_crash.inc
--error 0,CR_SERVER_LOST,ER_INTERNAL_ERROR
ALTER TABLESPACE mysql ENCRYPTION='Y';
--echo # Restart after crash
--source include/start_mysqld_no_echo.inc
SET debug='+d,skip_dd_table_access_check';

--echo # Wait for Encryption processing to finish in background thread
let $wait_condition = SELECT ENCRYPTION = 'Y'
        FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
        WHERE NAME='mysql';
--source include/wait_condition.inc

# Make sure ts file is updated with new records in table
set global innodb_buf_flush_list_now = 1;

--echo # After restart/recovery, check that Encryption was roll-forward
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME='mysql';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE NAME='mysql';

# Try to do encryption one more time (dummy). Should not do anything.
ALTER TABLESPACE mysql ENCRYPTION='Y';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME='mysql';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE NAME='mysql';

--echo #########################################################################
--echo # RESTART 2 : WITH KEYRING
--echo #########################################################################
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc
SET debug='+d,skip_dd_table_access_check';

SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME='mysql';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE NAME='mysql';

--echo ############################################################
--echo # ALTER TABLESPACE 2 :    Encrypted => Unencrypted         #
--echo #                         (crash at page 10)               #
--echo ############################################################
--echo # Set Unencryption process to crash at page 10
SET SESSION debug= '+d,alter_encrypt_tablespace_page_10';

--echo # Unencrypt the tablespace. It will cause crash.
--source include/expect_crash.inc
--error 0,CR_SERVER_LOST,ER_INTERNAL_ERROR
ALTER TABLESPACE mysql ENCRYPTION='N';
--echo # Restart after crash
--source include/start_mysqld_no_echo.inc
SET debug='+d,skip_dd_table_access_check';

--echo # Wait for Unencryption processing to finish in background thread
let $wait_condition = SELECT ENCRYPTION = 'N'
        FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
        WHERE NAME='mysql';
--source include/wait_condition.inc

# Make sure ts file is updated with new records in table
set global innodb_buf_flush_list_now = 1;

--echo # After restart/recovery, check that Unencryption was roll-forward
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME='mysql';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE NAME='mysql';

# Try to do unencryption one more time (dummy). Should not do anything.
ALTER TABLESPACE mysql ENCRYPTION='N';
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME='mysql';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE NAME='mysql';

--echo #########################################################################
--echo # RESTART 3 : WITHOUT KEYRING
--echo #########################################################################
--source include/keyring_tests/helper/instance_backup_manifest.inc
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc
SET debug='+d,skip_dd_table_access_check';

SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME='mysql';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE NAME='mysql';

--echo #############################################################
--echo # TEST 3 : CRASH BEFORE/AFTER ENCRYPTION PROCESSING.
--echo #############################################################
--echo
--echo #########################################################################
--echo # RESTART 4 : WITH KEYRING
--echo #########################################################################
--source include/keyring_tests/helper/instance_restore_manifest.inc
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc
SET debug='+d,skip_dd_table_access_check';

# Encrypt tablespace
ALTER TABLESPACE mysql ENCRYPTION='Y';
# Read rows from table
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE NAME='mysql';

--echo # Set server to crash just before encryption processing starts
SET SESSION debug="+d,alter_encrypt_tablespace_crash_before_processing";

--echo # Unencrypt the tablespace. It will cause crash.
--source include/expect_crash.inc
--error 0,CR_SERVER_LOST,ER_INTERNAL_ERROR
ALTER TABLESPACE mysql ENCRYPTION='N';
--echo # Restart after crash
--source include/start_mysqld_no_echo.inc
SET debug='+d,skip_dd_table_access_check';

--echo # Wait for Unencryption processing to finish in background thread
let $wait_condition = select count(*) = 0
        from performance_schema.events_stages_current
        where EVENT_NAME='stage/innodb/alter tablespace (encryption)';
--source include/wait_condition.inc

# Encrytion property of tablespace shouldn't have changed i.e. it should still
# be encrypted.
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME='mysql';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE NAME='mysql';


--echo # Set server to crash just after encryption processing finishes
SET SESSION debug="-d,alter_encrypt_tablespace_crash_before_processing";
SET SESSION debug="+d,alter_encrypt_tablespace_crash_after_processing";

--echo # Unencrypt the tablespace. It will cause crash.
--source include/expect_crash.inc
--error 0,CR_SERVER_LOST,ER_INTERNAL_ERROR
ALTER TABLESPACE mysql ENCRYPTION='N';
--echo # Restart after crash
--source include/start_mysqld_no_echo.inc
SET debug='+d,skip_dd_table_access_check';

--echo # Wait for Unencryption processing to finish in background thread
let $wait_condition = select count(*) = 0
        from performance_schema.events_stages_current
        where EVENT_NAME='stage/innodb/alter tablespace (encryption)';
--source include/wait_condition.inc

# Encrytion property of tablespace should have changed i.e. it should be
# unencrypted now.
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME='mysql';
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE NAME='mysql';

--echo #############################################################
--echo # TEST 4 : CRASH DURING KEY ROTATION.
--echo #############################################################
--echo
--echo #########################################################################
--echo # RESTART 5 : WITH KEYRING
--echo #########################################################################
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc
SET debug='+d,skip_dd_table_access_check';

# Encrypt tablespace
ALTER TABLESPACE mysql ENCRYPTION='Y';
# Read rows from table
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE NAME='mysql';

--echo # Set server to crash while rotating encryption
SET SESSION debug="+d,ib_crash_during_rotation_for_encryption";

# Rotate the key. It will cause server to crash.
--source include/expect_crash.inc
--error 0,CR_SERVER_LOST,ER_INTERNAL_ERROR
ALTER INSTANCE ROTATE INNODB MASTER KEY;

--echo # Restart after crash
--source include/start_mysqld_no_echo.inc
SET debug='+d,skip_dd_table_access_check';
# Read rows from table
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE NAME='mysql';

# Rotate the key.
SET SESSION debug="-d,ib_crash_during_rotation_for_encryption";
ALTER INSTANCE ROTATE INNODB MASTER KEY;
# Read rows from table
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE NAME='mysql';

--echo #############################################################
--echo # TEST 5 : PRIVILEGE CHECK.
--echo #############################################################
--echo

CREATE DATABASE priv_test;
CREATE USER myuser@'localhost';
GRANT ALL ON priv_test.* TO myuser@'localhost';

--echo #connection con1
--connect(con1,localhost,myuser,,priv_test)
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
ALTER TABLESPACE mysql ENCRYPTION='Y';

--disconnect con1
--source include/wait_until_disconnected.inc

--echo #connection default
--connection default
--error ER_WRONG_USAGE
GRANT CREATE TABLESPACE ON mysql.* TO myuser@'localhost';
GRANT CREATE TABLESPACE ON *.* TO myuser@'localhost';

--echo #connection con1
--connect(con1,localhost,myuser,,priv_test)
ALTER TABLESPACE mysql ENCRYPTION='N';

--echo #connection default
--connection default
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE NAME='mysql';

--echo #connection con1
--connection con1
ALTER TABLESPACE mysql ENCRYPTION='Y';

--echo #connection default
--connection default
SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE NAME='mysql';

--disconnect con1
DROP DATABASE priv_test;
DROP USER myuser@localhost;

--echo ###########
--echo # Cleanup #
--echo ###########
# Unencrypt tablespace
ALTER TABLESPACE mysql ENCRYPTION='N';

SELECT NAME,OPTIONS FROM mysql.tablespaces WHERE NAME='mysql';
