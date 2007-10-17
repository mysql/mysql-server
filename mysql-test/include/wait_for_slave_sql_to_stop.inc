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
####################################################
connection slave;
let $row_number= 1;
let $run= 1;
let $counter= 300;

while ($run)
{
  let $sql_result= query_get_value("SHOW SLAVE STATUS", Slave_SQL_Running, $row_number);
  if (`SELECT '$sql_result' = 'No'`){
    let $run= 0;
  }
  sleep 0.1;
  if (!$counter){
    --echo "Failed while waiting for slave SQL thread to stop"
    query_vertical SHOW SLAVE STATUS;
    exit;
  }
  dec $counter;
}
