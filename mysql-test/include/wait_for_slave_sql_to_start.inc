###################################################
#Author:  Mats (based on file written by Jeb)
#Date:    2008-05-06
#Purpose: To wait for slave SQL thread to start
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
  let $sql_result= query_get_value("SHOW SLAVE STATUS",  Slave_SQL_Running, $row_number);
  if (`SELECT '$sql_result' = 'Yes'`){
    let $run= 0;
  }
  sleep 0.1;
  if (!$counter){
    --echo "Failed while waiting for slave SQL to start"
    query_vertical SHOW SLAVE STATUS;
    exit;
  }
  dec $counter;
}


