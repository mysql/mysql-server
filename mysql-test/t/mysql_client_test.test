--source include/no_valgrind_without_big.inc
--source include/force_myisam_default.inc
--source include/have_myisam.inc

# need to have the dynamic loading turned on for the client plugin tests
--source include/have_plugin_auth.inc

SET @old_general_log= @@global.general_log;
SET @old_slow_query_log= @@global.slow_query_log;
SET @old_log_output = @@global.log_output;
SET @old_general_log_file = @@global.general_log_file;

--let $LOG_DIR= $MYSQLTEST_VARDIR/log

SET GLOBAL log_output="FILE,TABLE";
--replace_result $LOG_DIR LOG_DIR
eval SET GLOBAL general_log_file = '$LOG_DIR/master.log';
SET GLOBAL general_log= 'ON';

#
# If this test fails with "command "$MYSQL_CLIENT_TEST" failed",
# you should either run mysql_client_test separately against a running
# server or run mysql-test-run --debug mysql_client_test and check
# var/log/mysql_client_test.trace

--exec echo "$MYSQL_CLIENT_TEST" > $LOG_DIR/mysql_client_test.out.log 2>&1
--exec $MYSQL_CLIENT_TEST --getopt-ll-test=25600M $PLUGIN_AUTH_CLIENT_OPT >> $LOG_DIR/mysql_client_test.out.log 2>&1

# End of 4.1 tests
echo ok;

# File 'test_wl4435.out.log' is created by mysql_client_test.cc
--echo
--echo # cat MYSQL_TMP_DIR/test_wl4435.out.log
--echo # ------------------------------------
--cat_file $MYSQL_TMP_DIR/test_wl4435.out.log
--echo # ------------------------------------
--echo

SET @@global.general_log= @old_general_log;
SET @@global.slow_query_log= @old_slow_query_log;
SET @@global.log_output= @old_log_output;
SET @@global.general_log_file = @old_general_log_file;
TRUNCATE TABLE mysql.general_log;
TRUNCATE TABLE mysql.slow_log;

--echo #
--echo # Bug#24963580 INFORMATION_SCHEMA:MDL_REQUEST::
--echo # INIT_WITH_SOURCE(MDL_KEY::ENUM_MDL_NAMESPACE
--echo #

let BASEDIR=    `select @@basedir`;
let DDIR=       $MYSQL_TMP_DIR/lctn_test;
let MYSQLD_LOG= $MYSQL_TMP_DIR/server.log;
let extra_args= --no-defaults --innodb_dedicated_server=OFF --log-error=$MYSQLD_LOG --loose-skip-auto_generate_certs --loose-skip-sha256_password_auto_generate_rsa_keys --tls-version= --lower_case_table_names=1 --basedir=$BASEDIR --lc-messages-dir=$MYSQL_SHAREDIR;
let BOOTSTRAP_SQL= $MYSQL_TMP_DIR/tiny_bootstrap.sql;

--echo # Shut server down.
--let $shutdown_server_timeout= 300
--source include/shutdown_mysqld.inc

--echo # Create bootstrap file.
write_file $BOOTSTRAP_SQL;
  CREATE SCHEMA test;
EOF

--echo # First start the server with --initialize
--exec $MYSQLD $extra_args --secure-file-priv="" --initialize-insecure --datadir=$DDIR --init-file=$BOOTSTRAP_SQL

--echo # Restart the server against DDIR.
--let $restart_parameters= restart: --datadir=$DDIR --lower_case_table_names=1 --secure-file-priv= --no-console --log-error=$MYSQLD_LOG
--replace_result $DDIR DDIR $MYSQLD_LOG MYSQLD_LOG
--source include/start_mysqld.inc

--echo # Shutdown server.
--let $shutdown_server_timeout= 300
--source include/shutdown_mysqld.inc

--echo # Cleanup.
--echo #   Delete \$DDIR.
--force-rmdir $DDIR
--echo #   Delete sql files.
--remove_files_wildcard $MYSQL_TMP_DIR *.sql
--echo #   Delete log files.
--remove_files_wildcard $MYSQL_TMP_DIR *.log

--echo # Restart server without --lower-case-table-names
--let $restart_parameters= restart:
--source include/start_mysqld.inc
