# ==== Purpose ====
#
# Shutdown a server and start it with mysql_safe
#
# ==== Usage ====
#
# [--let $plugin_list= "plugin.so"]
# [--let $mysqld_extra_settings= "--skip-replica-start=0"]
# --source include/restart_with_mysqld_safe.inc
#
# Parameters:
#   $plugin_list
#     the list of plugins to load.
#   $mysqld_extra_settings
#     settings passed into mysqld_safe for the server

# This file cannot be used on windows
--source include/not_windows.inc

# 1) Get settings as mysql_safe does not accepts --defaults-group-suffix="suffix"

let $MYSQLD_DATADIR= `SELECT @@datadir`;
let $MYSQL_BASEDIR= `SELECT @@basedir`;
let $MYSQL_PLUGINDIR= `SELECT @@plugin_dir`;
let $MYSQL_SOCKET= `SELECT @@socket`;
let $MYSQL_TIMEZONE= `SELECT @@time_zone`;
let $MYSQL_PIDFILE= `SELECT @@pid_file`;
let $MYSQL_PORT= `SELECT @@port`;
let $MYSQL_MESSAGESDIR= `SELECT @@lc_messages_dir`;
let $MYSQLX_PORT= `SELECT @@mysqlx_port`;
let $MYSQLX_SOCKET= `SELECT @@mysqlx_socket`;
let $start_page_size= `select @@innodb_page_size`;
let $other_page_size_k= `SELECT $start_page_size DIV 1024`;
let $other_page_size_nk= `SELECT CONCAT($other_page_size_k,'k')`;

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

--exec sh $MYSQLD_SAFE --defaults-file=$MYSQLTEST_VARDIR/my.cnf --server_id=$_server_id --log-error=$MYSQLTEST_VARDIR/log/mysqld.$_server_id.err --basedir=$MYSQL_BASEDIR --ledir=$mysqld_path --mysqld=$mysqld_bin --datadir=$MYSQLD_DATADIR --socket=$MYSQL_SOCKET --mysqlx_socket=$MYSQLX_SOCKET --pid-file=$MYSQL_PIDFILE --port=$MYSQL_PORT --mysqlx_port=$MYSQLX_PORT --timezone=SYSTEM --log-output=file --loose-debug-sync-timeout=600 --default-storage-engine=InnoDB --default-tmp-storage-engine=InnoDB  --secure-file-priv="" --core-file --lc-messages-dir=$MYSQL_MESSAGESDIR --innodb-page-size=$other_page_size_nk --log-error-verbosity=3 --plugin-dir=$MYSQL_PLUGINDIR --plugin-load=$plugin_list $mysqld_extra_settings  < /dev/null > /dev/null 2>&1 &

--enable_reconnect
--source include/wait_until_connected_again.inc
--disable_reconnect

--let $rpl_server_number= $_server_id
--source include/rpl_reconnect.inc
