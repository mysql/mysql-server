--source include/big_test.inc

let BASEDIR= `select @@basedir`;
let DDIR=$MYSQL_TMP_DIR/installdb_test;
let MYSQLD_LOG=$MYSQL_TMP_DIR/server.log;
let extra_args=--no-defaults --innodb_dedicated_server=OFF --console --tls-version= --loose-skip-auto_generate_certs --loose-skip-sha256_password_auto_generate_rsa_keys --basedir=$BASEDIR --lc-messages-dir=$MYSQL_SHAREDIR;

--echo # shut server down
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc
--echo # Server is down


--echo #
--echo # Bug#35254025: Mysql instance cannot be initialized correctly because autocommit is turned off.
--echo #

--write_file $MYSQL_TMP_DIR/bug35254025init.sql
CREATE DATABASE test;
EOF

--echo # Run the server with --initialize --autocommit=off
--exec $MYSQLD $extra_args --initialize-insecure $VALIDATE_PASSWORD_OPT --datadir=$DDIR --init-file=$MYSQL_TMP_DIR/bug35254025init.sql --autocommit=off > $MYSQLD_LOG 2>&1

remove_file $MYSQL_TMP_DIR/bug35254025init.sql;

--echo # Restart the server against DDIR
--exec echo "restart:--datadir=$DDIR $VALIDATE_PASSWORD_OPT" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc

--echo # connect as root
connect(root_con,localhost,root,,mysql);

--echo # Test: must show sys
SHOW DATABASES LIKE 'sys';

--let $assert_text=Checking for the autocommit ignore warning
--let $assert_file=$MYSQLD_LOG
--let $assert_select=Ignoring a non-default value for --autocommit during --initialize
--let $assert_count=1
--source include/assert_grep.inc

--echo # shut server down
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc
--echo # Server is down

--echo # close the test connection
connection default;
disconnect root_con;

--echo # delete mysqld log
remove_file $MYSQLD_LOG;

--echo # delete datadir
--force-rmdir $DDIR

--echo #
--echo # Cleanup
--echo #
--echo # Restarting the server
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc

--echo # End of 8.0 tests
