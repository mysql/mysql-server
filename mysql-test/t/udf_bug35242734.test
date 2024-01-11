#
# Bug#35242734: Signal 11 due to missing UDF file
#
# Test that server correctly handles the call to invalid
# UDF function, even after the server was restarted.
#
# Server restart code is based on main.restart_server test
# (using RESTART command with server started by supervisor process)
# because restarting through standard MTR mechanisms reinitializes the
# mysql.func table content (and this test needs the content
# to be preserved after restart).
#

# Hangs forever with ASAN and gcc >= 10.0
--source include/not_asan.inc
--source include/have_mysqld_safe.inc
--source include/not_windows.inc

--echo # Verifying RESTART using mysqld_safe as supervisor process.

# 1) Set variables to be used in parameters of mysqld_safe.
let $MYSQLD_DATADIR= `SELECT @@datadir`;
let $MYSQL_BASEDIR= `SELECT @@basedir`;
let $MYSQL_SOCKET= `SELECT @@socket`;
let $MYSQL_TIMEZONE= `SELECT @@time_zone`;
let $MYSQL_PIDFILE= `SELECT @@pid_file`;
let $MYSQL_PORT= `SELECT @@port`;
let $MYSQL_MESSAGESDIR= `SELECT @@lc_messages_dir`;
let $start_page_size= `select @@innodb_page_size`;
let $other_page_size_k=    `SELECT $start_page_size DIV 1024`;
let $other_page_size_nk=       `SELECT CONCAT($other_page_size_k,'k')`;

# mysqld_path to be passed to --ledir
# use test;
perl;
 my $dir = $ENV{'MYSQLTEST_VARDIR'};
 open ( OUTPUT, ">$dir/tmp/mysqld_path_file.inc") ;
 my $path = $ENV{MYSQLD};
 $path =~ /^(.*)\/([^\/]*)$/;
 print OUTPUT "let \$mysqld_path = $1;\n";
 print OUTPUT "let \$mysqld_bin = $2;\n";
 close (OUTPUT);
EOF

#Get the value of the variable from to MTR, from perl
--source  $MYSQLTEST_VARDIR/tmp/mysqld_path_file.inc
#Remove the temp file
--remove_file $MYSQLTEST_VARDIR/tmp/mysqld_path_file.inc

# 2) Shutdown mysqld which is started by mtr.
--let $_server_id= `SELECT @@server_id`
--let $_expect_file_name= $MYSQLTEST_VARDIR/tmp/mysqld.$_server_id.expect
--exec echo "wait" > $_expect_file_name
--shutdown_server
--source include/wait_until_disconnected.inc

# 3) Run the mysqld_safe script with exec.
--exec sh $MYSQLD_SAFE --defaults-file=$MYSQLTEST_VARDIR/my.cnf --log-error=$MYSQLTEST_VARDIR/log/err.log --basedir=$MYSQL_BASEDIR --ledir=$mysqld_path --mysqld=$mysqld_bin --datadir=$MYSQLD_DATADIR --socket=$MYSQL_SOCKET --pid-file=$MYSQL_PIDFILE --port=$MYSQL_PORT --timezone=SYSTEM --log-output=file --loose-debug-sync-timeout=600 --default-storage-engine=InnoDB --default-tmp-storage-engine=InnoDB  --secure-file-priv="" --loose-skip-log-bin --core-file --lc-messages-dir=$MYSQL_MESSAGESDIR --innodb-page-size=$other_page_size_nk < /dev/null > /dev/null 2>&1 &
# mysqld_safe takes some time to start mysqld
--source include/wait_until_connected_again.inc

# 4) Reconnect to mysqld again
connection default;

# 5) Setup actual test and verify server correctly handles call to invalid UDF
eval INSERT INTO mysql.func(name, ret, dl, type) VALUES ('udf_test', 0, 'should_not_parse.so', 'function');
USE mysql;
--error ER_SP_DOES_NOT_EXIST
SELECT udf_test('test');
# verify UDF registration visible before the restart
SELECT * from mysql.func;

# 6) Execute the RESTART sql command.
--exec $MYSQL -h localhost -S $MYSQL_SOCKET -P $MYSQL_PORT -u root -e "RESTART" 2>&1
--source include/wait_until_disconnected.inc
# mysqld_safe takes some time to restart mysqld
--source include/wait_until_connected_again.inc
--echo # Executing a sql command after RESTART.

# 7) Main test part, verify server correctly handles call to invalid UDF after the restart
# verify UDF registration survived the server restart
SELECT * from mysql.func;
# verify invalid UDF call still fails
--error ER_SP_DOES_NOT_EXIST
SELECT udf_test('test');
# cleanup
DELETE FROM mysql.func WHERE name='udf_test';

# 8) Shutdown mysqld with mysqladmin
--exec $MYSQLADMIN -h localhost -S $MYSQL_SOCKET -P $MYSQL_PORT -u root shutdown 2>&1
# Delay introduced - mysqld_safe takes some time to restart mysqld
--source include/wait_until_disconnected.inc

# 9) Restart mysqld of mtr
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc
