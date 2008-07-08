###################################################
#Author: Jeb
#Date: 2007-06-11
#Purpose: used for SQL errors on the slave. If Slave gets a sql
#         error, the SQL trhead should stop
#Details:
#      1) Fill in and setup variables
#      2) loop through looking for
#         sql threads to stop
#      3) If loops too long die.
#
# INPUT:
#    $timeout_counter    Number of 1/10 sec to wait
#    $keep_connection    Do not do any "connect"
####################################################
if (!$keep_connection)
{
  connection slave;
}
let $row_number= 1;
let $run= 1;
let $counter = $timeout_counter;
if (!$counter)
{
  let $counter= 3000;
}

while ($run)
{
  if (!$keep_connection)
  {
    if (!$counter)
    {
      let $binlog_pos = query_get_value("SHOW SLAVE STATUS", Exec_Master_Log_Pos, 1);
      let $binlog_file = query_get_value("SHOW SLAVE STATUS", Master_Log_File, 1);
    }
  }
  let $sql_result= query_get_value("SHOW SLAVE STATUS", Slave_SQL_Running, $row_number);
  if (`SELECT '$sql_result' = 'No'`){
    let $run= 0;
  }
  sleep 0.1;
  if (!$counter){
    --echo "Failed while waiting for slave SQL thread to stop"
    if (!$keep_connection)
    {
      --echo [on master]
      connection master;
      --echo **** Note that the binlog positions are not read atomically:  ****
      --echo **** the replication might in reality have progressed further ****
      eval SHOW BINLOG EVENTS IN '$binlog_file' FROM $binlog_pos LIMIT 5;
      --echo [on slave]
      connection slave;
    }
    --echo **** Note that the process list might have changed since the    ****
    --echo **** failure was detected                                       **** 
    SHOW PROCESSLIST;
    --echo **** Note that the binlog positions are not read atomically, so ****
    --echo **** there is a small risk that the binlog status is incorrect  ****
    query_vertical SHOW SLAVE STATUS;
    exit;
  }
  dec $counter;
}
