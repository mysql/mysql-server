--source include/not_windows.inc
--source include/not_valgrind.inc
--source include/not_asan.inc
--source include/big_test.inc
--source include/have_component_keyring_file.inc
--source suite/component_keyring_file/inc/setup_component.inc

# Set variables to be used in parameters of mysqld.
let $MYSQLD_DATADIR= `SELECT @@datadir`;
let $MYSQL_BASEDIR= `SELECT @@basedir`;
let $MYSQL_SOCKET= `SELECT @@socket`;
let $MYSQL_PIDFILE= `SELECT @@pid_file`;
let $MYSQL_PORT= `SELECT @@port`;
let $MYSQL_MESSAGESDIR= `SELECT @@lc_messages_dir`;

--echo # Stop server
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/shutdown_mysqld.inc

--write_file $MYSQLTEST_VARDIR/tmp/my.cnf
[mysqld]
innodb_fast_shutdown=1
EOF

--let $MYSQLD_LOG= $MYSQL_TMP_DIR/server.log

--echo # Start server with wrong basedir and without plugin-dir, keyring component load should fail and hence server should fail to start
--error 1
--exec $MYSQLD --defaults-file=$MYSQLTEST_VARDIR/tmp/my.cnf --log-error=$MYSQLD_LOG --basedir=WRONG_BASEDIR --datadir=$MYSQLD_DATADIR --socket=$MYSQL_SOCKET --pid-file=$MYSQL_PIDFILE --port=$MYSQL_PORT --lc-messages-dir=$MYSQL_MESSAGESDIR --daemonize --log-error-verbosity=3

--replace_result $PLUGIN_DIR_OPT PLUGIN_DIR_OPT
--source include/start_mysqld.inc

--source suite/component_keyring_file/inc/teardown_component.inc

