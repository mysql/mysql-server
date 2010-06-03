# Include file to decrease test code duplication

--eval $create_statement
--eval $insert_statement
--echo # State before crash
--list_files $DATADIR/test
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM t1;
--write_file $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
wait
EOF
# CR_SERVER_LOST
--error 2013
--eval $crash_statement
--echo # State after crash (before recovery)
--list_files_write_file $MYSQLTEST_VARDIR/tmp/ls_of_crash.txt $DATADIR/test
--let LS_FILE=$MYSQLTEST_VARDIR/tmp/ls_of_crash.txt
--let EXPECT_FILE=$MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--perl
open(FH, "<", $ENV{'LS_FILE'}) or die "Failed to open ls file";
while (<FH>)
{
  s/sqlx.*\./sqlx-nnnn_nnnn./;
  print;
}
close(FH);
unlink $ENV{'LS_FILE'};
open(FH, ">", $ENV{'EXPECT_FILE'}) or die "Failed to open expect file";
print FH "restart";
close(FH);
EOF
--enable_reconnect
--source include/wait_until_connected_again.inc
--echo # State after crash recovery
--list_files $DATADIR/test
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM t1;
DROP TABLE t1;
