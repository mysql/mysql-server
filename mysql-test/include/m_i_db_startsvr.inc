--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server 10
--source include/wait_until_disconnected.inc

--perl
use File::Path 'rmtree';
$DDIR=$ENV{"MYSQL_TMP_DIR"}."/installdb_test";
rmtree([ "$DDIR" ]);
EOF

--echo # Execution of mysql_install_db starts............
--exec $BASEDIR/bin/mysql_install_db --no-defaults --datadir=$DDIR --basedir=$BASEDIR --extra-sql-file=$MYSQL_TEST_DIR/t/m_i_db_createschema.sql --defaults-file=$MYSQL_TEST_DIR/t/m_i_db_config.cnf $midparam
--echo # Execution of mysql_install_db ends..............
--exec echo "restart:--datadir=$DDIR " > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--enable_reconnect
--source include/wait_until_connected_again.inc

--perl
use Fcntl;
use Tie::File;

$passwd="";
tie my @rows, 'Tie::File', "$ENV{MYSQL_TMP_DIR}/readpwdfilepath.txt", mode => O_RDONLY or die "error: $!\n";
$filename=$rows[-1];
if (-e $filename) {
  # open the file in read-only mode as we won't be modifying the file
  tie my @rows, 'Tie::File', $filename, mode => O_RDONLY or die "error: $!\n";
  #-- print last line
  $passwd=$rows[-1];
  # unlink $filename;
}
open(FILE, ">", "$ENV{MYSQL_TMP_DIR}/readpwd.inc") or die $!;
print FILE "delimiter lessprobability;\n";
print FILE "let passwd=$passwd";
print FILE "lessprobability\n";
print FILE "--delimiter ;\n";
close FILE;
EOF
--source $MYSQL_TMP_DIR/readpwd.inc
