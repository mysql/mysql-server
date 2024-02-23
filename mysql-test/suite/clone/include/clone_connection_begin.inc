## Create connection to clone instance optionally restarting instance with
## monitoring process.

# These variables can to be set before sourcing this file. Currently we used
# to test both with and without monitoring process.
#
# 1. Need to restart mysqld with monitoring process
# --let inst_monitor = 1
#
# 2. Mysqld server instance number for clone
# --let clone_inst_number = <instance number> 1/2/3 ...
#     - Instances must be configured in .cnf
#     - SERVER_PORT_[n] ENV must be set to server PORT in .cnf
#
# 3. Mysqld user name for connecting to the instance.
# --let clone_user = <user_name>
#
# 4. Number of connections to be created to the instance
# --let clone_connections = <number of connections> 1/2/3 ...
# Connections have the name as clone_conn_<number>
# e.g. clone_conn_1, clone_conn_2, clone_conn_3 etc.
#
# This script should be used in pair with clone_connection_end.inc
# --source clone_connection_begin.inc
# ...
# --source clone_connection_end.inc

if (!$clone_user) {
  --let $clone_user = 'root'
}

if (!$clone_connections) {
  --let $clone_connections = 1
}

if ($clone_inst_number) {
  --let $clone_port= \$SERVER_PORT_$clone_inst_number
  --connect (clone_conn_1, 127.0.0.1, $clone_user,,test,$clone_port)
}

if (!$clone_inst_number) {
  --connection default
}

# Get Server ID
--let $SERVER_ID= `SELECT @@server_id`

if ($clone_inst_number) {
  if ($SERVER_ID == 1) {
    --disconnect clone_conn_1
    --connection default
  }
}

if ($SERVER_ID != 1) {
  --echo Install clone plugin on recipient server
  --replace_result $CLONE_PLUGIN CLONE_PLUGIN
  --eval INSTALL PLUGIN clone SONAME '$CLONE_PLUGIN'
}

# A. Check if we need monitoring process. Taken from mysqld_safe.test
if ($inst_monitor) {

  # 1. Set variables to be used in parameters of mysqld_safe.
  --let $SERVER_ID= `SELECT @@server_id`
  --let $MYSQLD_DATADIR= `SELECT @@datadir`
  --let $MYSQL_BASEDIR= `SELECT @@basedir`
  --let $MYSQL_MESSAGESDIR= `SELECT @@lc_messages_dir`

  --let $MYSQL_PIDFILE= `SELECT @@pid_file`
  --let $MYSQL_SOCKET= `SELECT @@socket`
  --let $MYSQLX_SOCKET= `SELECT @@mysqlx_socket`
  --let $MYSQL_PORT= `SELECT @@port`
  --let $MYSQLX_PORT= `SELECT @@mysqlx_port`
  --let $MYSQL_PLUGIN_DIR = `SELECT @@plugin_dir`

  --let $page_size= `select @@innodb_page_size`
  --let $error_verbosity = `select @@log_error_verbosity`

  # mysqld_path to be passed to --ledir
  perl;
    my $dir = $ENV{'MYSQLTEST_VARDIR'};
    open ( OUTPUT, ">$dir/tmp/mysqld_path_file.inc") ;
    my $path = $ENV{MYSQLD};
    $path =~ /^(.*)\/([^\/]*)$/;
    print OUTPUT "let \$mysqld_path = $1;\n";
    print OUTPUT "let \$mysqld_bin = $2;\n";
    close (OUTPUT);
  EOF

  # Get the value of the variable to MTR, from perl
  --source  $MYSQLTEST_VARDIR/tmp/mysqld_path_file.inc

  # Remove the temp file
  --remove_file $MYSQLTEST_VARDIR/tmp/mysqld_path_file.inc

  # 2. Shutdown mysqld instance which is started by mtr.
  --let $_expect_file_name= $MYSQLTEST_VARDIR/tmp/mysqld.$SERVER_ID.expect
  --exec echo "wait" > $_expect_file_name
  --shutdown_server
  --source include/wait_until_disconnected.inc

  # 3. Run mysqld_safe script
  --exec sh $MYSQLD_SAFE --defaults-file=$MYSQLTEST_VARDIR/my.cnf --server-id=$SERVER_ID --log-error=$MYSQLTEST_VARDIR/log/mysqld.$SERVER_ID.err --log-error-verbosity=$error_verbosity --basedir=$MYSQL_BASEDIR --ledir=$mysqld_path --mysqld=$mysqld_bin --datadir=$MYSQLD_DATADIR --socket=$MYSQL_SOCKET --mysqlx_socket=$MYSQLX_SOCKET --pid-file=$MYSQL_PIDFILE --port=$MYSQL_PORT --mysqlx_port=$MYSQLX_PORT --plugin_dir=$MYSQL_PLUGIN_DIR --timezone=SYSTEM --log-output=file --secure-file-priv="" --core-file --lc-messages-dir=$MYSQL_MESSAGESDIR --innodb-page-size=$page_size < /dev/null > /dev/null 2>&1 &
  --source include/wait_until_connected_again.inc
}

# B. Create the connections
--let $conn_nummber = 1

# For non default server, we have already created the first connection
if ($SERVER_ID != 1) {
  --let $conn_nummber = 2

  # Create some local table and tablespace in recipient
  CREATE TABLE t_recipient(col1 INT PRIMARY KEY, col2 int);
  INSERT INTO t_recipient VALUES(1, 10);
  CREATE UNDO TABLESPACE undo_005 ADD DATAFILE 'undo_005.ibu';
  CREATE TABLESPACE tbs1_recipient ADD DATAFILE 'tbs1_data1.ibd';
}

while ($conn_nummber <= $clone_connections)
{
  --let $conn_name = clone_conn_$conn_nummber

  if ($clone_inst_number) {
    --let $clone_port= \$SERVER_PORT_$clone_inst_number
    --connect ($conn_name, 127.0.0.1, $clone_user,,test,$clone_port)
  }

  # Connect to the default instance if instance number is not provided.
  if (!$clone_inst_number) {
    --connect ($conn_name, localhost, $clone_user,,test)
  }
  --inc $conn_nummber
}
--connection default
