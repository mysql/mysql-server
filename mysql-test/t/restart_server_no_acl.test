--source include/big_test.inc
--echo #
--echo # WL#12971: Make MySQL stop if it can't source the privilege tables
--echo #

let BASEDIR= `select @@basedir`;
let DDIR=$MYSQL_TMP_DIR/installdb_test;
let MYSQLD_LOG=$MYSQL_TMP_DIR/server.log;
let extra_args=--no-defaults --innodb_dedicated_server=OFF --console --loose-skip-auto_generate_certs --loose-skip-sha256_password_auto_generate_rsa_keys --tls-version= --basedir=$BASEDIR --lc-messages-dir=$MYSQL_SHAREDIR --skip-mysqlx;
let BOOTSTRAP_SQL=$MYSQL_TMP_DIR/tiny_bootstrap.sql;
let $MYSQLD_SOCKET= `SELECT @@socket`;
let $MYSQLD_PORT= `SELECT @@port`;

write_file $BOOTSTRAP_SQL;
CREATE DATABASE test;
EOF

--echo # Shut the test server down
--source include/shutdown_mysqld.inc

--exec $MYSQLD $extra_args --initialize-insecure --datadir=$DDIR --init-file=$BOOTSTRAP_SQL > $MYSQLD_LOG 2>&1

--echo # Restart the server against DDIR
--exec echo "restart:--datadir=$DDIR " > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc

connect(root_con,localhost,root,,mysql);
--echo now we drop mysql.user into the new ddir
DROP TABLE mysql.user;
connection default;

--echo # shut server down
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc
--echo # Server is down

--echo # try starting the server on a database not having mysql.user: must fail.
--error 1
--exec $MYSQLD $extra_args --console --gdb --datadir=$DDIR --port=$MYSQLD_PORT --socket=$MYSQLD_SOCKET >> $MYSQLD_LOG 2>&1

--echo #
--echo # Cleanup
--echo #
connection default;
disconnect root_con;

--echo # delete mysqld log
remove_file $MYSQLD_LOG;

--echo # delete bootstrap file
remove_file $BOOTSTRAP_SQL;

--echo # delete datadir
--force-rmdir $DDIR

--echo # Restart the server
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc


--echo # End of 8.0 tests
