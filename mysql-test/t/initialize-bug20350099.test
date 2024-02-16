--source include/big_test.inc
--source include/have_validate_password_plugin.inc

call mtr.add_suppression("Plugin validate_password reported: .Dictionary file not specified.");

let BASEDIR= `select @@basedir`;
let DDIR=$MYSQL_TMP_DIR/installdb_test;
let MYSQLD_LOG=$MYSQL_TMP_DIR/server.log;
let extra_args=--no-defaults --innodb_dedicated_server=OFF --console --loose-skip-auto_generate_certs --loose-skip-sha256_password_auto_generate_rsa_keys --tls-version= --basedir=$BASEDIR --lc-messages-dir=$MYSQL_SHAREDIR;
let PASSWD_FILE=$MYSQL_TMP_DIR/password_file.txt;

--echo # Save the count of columns in mysql
--let $mysql_cnt=`SELECT COUNT(COLUMN_NAME) FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA='mysql'`

--echo # shut server down
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc
--echo # Server is down

--echo #
--echo # Try --initialize
--echo #

--echo # Run the server with --initialize
--exec $MYSQLD $extra_args --initialize --datadir=$DDIR > $MYSQLD_LOG 2>&1

--echo extract the root password
--perl
  use strict;
  my $log= $ENV{'MYSQLD_LOG'} or die;
  my $passwd_file= $ENV{'PASSWD_FILE'} or die;
  my $FILE;
  open(FILE, "$log") or die;
  while (my $row = <FILE>)
  {
    if ($row =~ m/.*A temporary password is generated for root.localhost: ([^ \n][^ \n]*)/)
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

--echo # Restart the server against DDIR
--exec echo "restart:--datadir=$DDIR $VALIDATE_PASSWORD_OPT $VALIDATE_PASSWORD_LOAD --validate_password_policy=STRONG" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc

--echo # connect as root
connect(root_con,localhost,root,$new_pwd,mysql);

--echo # must fail due to password expiration
--error ER_MUST_CHANGE_PASSWORD
SELECT 1;

--echo # reset the password to the one set: should be strong enough so it wont fail
--disable_query_log
eval SET PASSWORD='$new_pwd';
--enable_query_log

--echo # Check the count of columns in mysql
--let $cnt=`SELECT COUNT(COLUMN_NAME) FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA='mysql';`
if ($cnt != $mysql_cnt)
{
--echo # Column count doesn't match. mtr=$mysql_cnt server=$cnt
--echo list columns in I_S.COLUMNS for the mysql db
SELECT TABLE_NAME,COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS
  WHERE TABLE_SCHEMA='mysql' ORDER BY TABLE_NAME,ORDINAL_POSITION;
}


--echo # check the user account
SELECT user, host, plugin, LENGTH(authentication_string)
  FROM mysql.user ORDER BY user;

--echo # shut server down
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc
--echo # Server is down

--echo # close the test connection
connection default;
disconnect root_con;

--echo # remove the password file
remove_file $PASSWD_FILE;

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

--echo # End of 5.7 tests
