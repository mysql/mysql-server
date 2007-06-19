###################################################
#Author: Jeb
#Date: 2007-06-11
#Purpose: To wait a brief time for slave to start
#Details:
#      1) Fill in and setup variables
#      2) loop through looking for both
#         io and sql threads to start
#      3) If loops too long die.
####################################################
connection slave;
let $row_number= 1;
let $run= 1;
let $counter= 300;

while ($run)
{
  let $io_result= query_get_value("SHOW SLAVE STATUS", Slave_IO_Running, $row_number);
  if (`SELECT '$io_result' = 'Yes'`){

    let $sql_result= query_get_value("SHOW SLAVE STATUS",  Slave_SQL_Running, $row_number);
    if (`SELECT '$sql_result' = 'Yes'`){
      let $run= 0;
    }
  }
  sleep 0.1;
  if (!$counter){
    --echo "Failed while waiting for slave to start"
    query_vertical SHOW SLAVE STATUS;
    exit;
  }
  dec $counter;
}


