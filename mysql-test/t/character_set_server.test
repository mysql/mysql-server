# This testcase is added as part of the fix for below bug:
# Bug#35529604 : Connect from OCI instance with PHP 5.6 to DBS (mysql 8.0.33) get the following
#                error: "PHP message: PHP Warning: mysql_connect(): Server sent charset (255)
#                unknown to the client"

--let $MYSQL_DATADIR  = `SELECT @@datadir`
--let $MYSQL_PORT     = `SELECT @@port`
--let $MYSQL_SOCKET   = `SELECT @@socket`
--let $MYSQL_BASEDIR  = `SELECT @@basedir`
--let $MYSQL_MSG_DIR  = `SELECT @@lc_messages_dir`

--echo # The character_set_server value is.
SHOW VARIABLES LIKE "%character_set_server%";

--echo
SET PERSIST character_set_server=greek;

--echo
--echo # Shutdown the server
--source include/stop_mysqld.inc

--echo # Start the server with "--collation-server=cp932_japanese_ci". Expectation : server startup fail.
--error 1
--exec $MYSQLD --datadir=$MYSQL_DATADIR --basedir=$MYSQL_BASEDIR --socket=$MYSQL_SOCKET --port=$MYSQL_PORT --lc-messages-dir=$MYSQL_MSG_DIR --secure-file-priv="" --collation-server=cp932_japanese_ci --log-error=$MYSQLTEST_VARDIR/log/character_set_server_err.log

--echo
--echo # Grep for this error message : "COLLATION 'cp932_japanese_ci' is not valid for CHARACTER SET 'greek'" in the log file.
--let $grep_file    = $MYSQLTEST_VARDIR/log/character_set_server_err.log
--let $grep_pattern = COLLATION 'cp932_japanese_ci' is not valid for CHARACTER SET 'greek'
--let $grep_output  = boolean
--source include/grep_pattern.inc

--echo
--source include/start_mysqld.inc

--echo # The character_set_server value is.
SHOW VARIABLES LIKE "%character_set_server%";

--echo
--echo # Clean up
RESET PERSIST character_set_server;
# Restart for mtr.
--let restart_parameters=
--source include/restart_mysqld.inc
