--echo ########################################################################
--echo # ALTER TABLESPACE :    Encrypted => Unencrypted                       #
--echo ########################################################################
--echo # Set process to crash at page 10

SET SESSION debug= '+d,alter_encrypt_tablespace_page_10';
--let $PLUGIN_DIR   =   `select @@plugin_dir`

SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME='encrypt_ts';

--echo # Unencrypt the tablespace. It will cause crash.
--source include/expect_crash.inc
--error 0,CR_SERVER_LOST,CR_SERVER_GONE_ERROR,ER_INTERNAL_ERROR
ALTER TABLESPACE encrypt_ts ENCRYPTION='N';

--echo # RESTART $rc : WITH KEYRING COMPONENT after crash and cause resume operation
--echo #              to crash $crash_point_desc
let $NEW_CMD = $MYSQLD_CMD --plugin-dir=$PLUGIN_DIR --log-error=$MYSQLTEST_VARDIR/log/my_restart.err --datadir=$MYSQLD_DATADIR --debug=+d,$crash_point;
--error 1,2,42,137
--exec $NEW_CMD

--inc $rc

# Following query should fail as server crashed during restart
--error CR_SERVER_GONE_ERROR,CR_SERVER_LOST
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME='encrypt_ts';

--echo # Search the pattern in error log
--source $MYSQLTEST_VARDIR/tmp/unencryption_error_log.inc

--echo # RESTART $rc : Normal. In this restart resume operation should finish successfully
let $restart_parameters = restart: --plugin-dir=$PLUGIN_DIR --log-error=$MYSQLTEST_VARDIR/log/my_restart.err;
--source include/start_mysqld_no_echo.inc
--inc $rc

--echo # Wait for Encryption processing to finish in background thread
let $wait_condition = SELECT ENCRYPTION = 'N'
        FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
        WHERE NAME='encrypt_ts';
--source include/wait_condition.inc

--echo # Search the pattern in error log
--source $MYSQLTEST_VARDIR/tmp/unencryption_error_log.inc

--echo # RESTART $rc : WITHOUT KEYRING COMPONENT
--source include/keyring_tests/helper/instance_backup_manifest.inc
let $restart_parameters = restart:;
--source include/restart_mysqld_no_echo.inc
--inc $rc

SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES WHERE NAME='encrypt_ts';
# We should be able to read t1 records even without keyring component as it should
# be unencrypted now.
SELECT * FROM t1 LIMIT 10;
