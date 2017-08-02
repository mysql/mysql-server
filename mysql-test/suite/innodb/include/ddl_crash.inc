
--disable_query_log
--eval $insert_statement
--enable_query_log

--sorted_result
--eval SELECT * FROM $table_name LIMIT 2;

let $did_crash= 0;

--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--disable_reconnect

# 2013 - CR_SERVER_LOST
--error 0,2013,ER_INTERNAL_ERROR
--eval $crash_statement
--echo # mysql_errno: $mysql_errno

if ($mysql_errno != 2013)
{
  --echo # No such crash injection, statement did not crash, cleaning up
  --remove_file $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
}

if ($mysql_errno == 2013)
{
  let $did_crash= 1;
  --exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
  --enable_reconnect
  let $WAIT_COUNT=6000;
  --source include/wait_time_until_connected_again.inc
  --eval CHECK TABLE $table_name
  --eval SHOW CREATE TABLE $table_name
  --sorted_result
  --eval SELECT * FROM $table_name LIMIT 2;
}

