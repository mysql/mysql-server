# WL#15369 Add progress information to the error log during shutdown
# We initialize, start and shutdown the server and verify if newly
# added logs as part of this WL appear in the intended way.

--source include/big_test.inc
--source include/have_innodb_16k.inc

let BASEDIR= `select @@basedir`;
let DDIR=$MYSQL_TMP_DIR/installdb_test;
let MYSQLD_LOG=$MYSQL_TMP_DIR/server.log;
let extra_args=--no-defaults --innodb_dedicated_server=OFF --console --loose-skip-auto_generate_certs --loose-skip-sha256_password_auto_generate_rsa_keys --tls-version= --basedir=$BASEDIR --lc-messages-dir=$MYSQL_SHAREDIR;
let PASSWD_FILE=$MYSQL_TMP_DIR/password_file.txt;

--echo # ---------------------------------------------------
--echo # shut server down

--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc
--echo # Server is down

--echo # ---------------------------------------------------

--echo # Run the server with --initialize
--exec $MYSQLD $extra_args --initialize --datadir=$DDIR --log-error-verbosity=3 > $MYSQLD_LOG 2>&1

--echo # ---------------------------------------------------

--echo # extract the root password

--perl
  use strict;
  my $log= $ENV{'MYSQLD_LOG'} or die;
  my $passwd_file= $ENV{'PASSWD_FILE'} or die;
  my $FILE;
  open(FILE, "$log") or die;
  while (my $row = <FILE>)
  {
    if ($row =~ m/.*\[Note\] \[[^]]*\] \[[^]]*\] A temporary password is generated for root.localhost: ([^ \n][^ \n]*)/)
    {
      my $passwd=$1;
      print "password found\n";
      my $OUT_FILE;
      open(OUT_FILE, "> $passwd_file");
      print OUT_FILE "delimiter lessprobability;\n";
      print OUT_FILE "let new_pwd=$passwd";
      print OUT_FILE "lessprobability\n";
      print OUT_FILE "--delimiter ;\n";
      close(OUT_FILE);
    }
  }
  close(FILE);
EOF

source $passwd_file;

--echo # ---------------------------------------------------

--echo # Restart the server against DDIR, and connect

--exec echo "restart:--datadir=$DDIR --log-error=$MYSQL_TMP_DIR/server.log" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc

--echo # connect as root
connect(root_con,localhost,root,$new_pwd,mysql);

--echo # reset the password
SET PASSWORD='';

CREATE DATABASE test;

--echo # ---------------------------------------------------
--echo # shut server down

--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc
--echo # Server is down

--echo # close the test connection
connection default;
disconnect root_con;

--echo # ---------------------------------------------------
--echo # Read the logs

let SEARCH_FILE= $MYSQL_TMP_DIR/server.log;

--echo # Looking for ER_SRV_INIT_START
let SEARCH_PATTERN= MySQL Server Initialization - start;
--source include/search_pattern.inc
--echo # Search completed

--echo # Looking for ER_SRV_INIT_END
let SEARCH_PATTERN= MySQL Server Initialization - end;
--source include/search_pattern.inc
--echo # Search completed

--echo # Looking for ER_SRV_START
let SEARCH_PATTERN= MySQL Server - start;
--source include/search_pattern.inc
--echo # Search completed

--echo # Looking for ER_CONNECTIONS_SHUTDOWN_START
let SEARCH_PATTERN= MySQL Server: Closing Connections - start;
--source include/search_pattern.inc
--echo # Search completed

--echo # Looking for ER_CONNECTIONS_SHUTDOWN_END
let SEARCH_PATTERN= MySQL Server: Closing Connections - end;
--source include/search_pattern.inc
--echo # Search completed

--echo # Looking for ER_PLUGINS_SHUTDOWN_START
let SEARCH_PATTERN= MySQL Server: Plugins Shutdown - start;
--source include/search_pattern.inc
--echo # Search completed

--echo # Looking for ER_PLUGINS_SHUTDOWN_END
let SEARCH_PATTERN= MySQL Server: Plugins Shutdown - end;
--source include/search_pattern.inc
--echo # Search completed

--echo # Looking for ER_COMPONENTS_INFRASTRUCTURE_SHUTDOWN_START
let SEARCH_PATTERN= MySQL Server: Components Shutdown - start;
--source include/search_pattern.inc
--echo # Search completed

--echo # Looking for ER_COMPONENTS_INFRASTRUCTURE_SHUTDOWN_END
let SEARCH_PATTERN= MySQL Server: Components Shutdown - end;
--source include/search_pattern.inc
--echo # Search completed

--echo # Looking for ER_SRV_END
let SEARCH_PATTERN= MySQL Server - end;
--source include/search_pattern.inc
--echo # Search completed

--echo # ---------------------------------------------------
--echo # Clean Up

--echo # delete mysqld log
remove_file $MYSQLD_LOG;

--echo # delete datadir
--force-rmdir $DDIR

--echo # delete password
remove_file $PASSWD_FILE;

--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc

--echo # ---------------------------------------------------

