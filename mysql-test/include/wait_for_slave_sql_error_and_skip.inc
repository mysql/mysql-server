# ==== Purpose ====
#
# Wait for slave SQL error, skip the erroneous statement and restart
# slave
#
# ==== Usage ====
#
# let show_sql_error=0|1;
# source include/wait_for_slave_sql_error_and_skip.inc;

echo --source include/wait_for_slave_sql_error_and_skip.inc;
connection slave;
source include/wait_for_slave_sql_error.inc;
if ($show_sql_error)
{
  let $error= query_get_value("SHOW SLAVE STATUS", Last_SQL_Error, 1);
  echo Last_SQL_Error = $error;
}

# skip the erroneous statement
set global sql_slave_skip_counter=1;
source include/start_slave.inc;
connection master;
