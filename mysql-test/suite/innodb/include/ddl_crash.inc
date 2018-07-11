
--disable_query_log
--eval $insert_statement
--disable_result_log

--eval SELECT * FROM $table_name LIMIT 1
--enable_result_log
--enable_query_log

if ($get_create_template)
{
  let $create_template= query_get_value(SHOW CREATE TABLE $table_name, Create Table, 1);
}

let $did_crash= 0;

--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--disable_reconnect

--error 0,CR_SERVER_LOST,ER_INTERNAL_ERROR
--eval $crash_statement

# 2013 - CR_SERVER_LOST
if ($mysql_errno != 2013)
{
  --echo # No such crash injection, statement did not crash, cleaning up
  --remove_file $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
}

# 2013 - CR_SERVER_LOST
if ($mysql_errno == 2013)
{
  let $did_crash= 1;
  --exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
  --enable_reconnect
  let $WAIT_COUNT=6000;
  --source include/wait_time_until_connected_again.inc
  let $check= query_get_value(CHECK TABLE $final_table_name, Msg_text, 1);
  if ($check != "OK")
  {
    --echo "Check table after recovery is not OK!"
  }
  let $create = query_get_value(SHOW CREATE TABLE $final_table_name, Create Table, 1);
  if ($create != $create_template)
  {
    --echo "Table definition is not as expected!"
  }
  # A full scan would be nice, no need to check the result
  --disable_query_log
  --disable_result_log
  --eval SELECT * FROM $final_table_name
  --enable_result_log
  --enable_query_log
}
