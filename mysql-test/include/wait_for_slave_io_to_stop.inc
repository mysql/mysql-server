###################################################
#Author: Jeb
#Date: 2007-06-11
#Purpose: used for io errors on the slave. If Slave gets an io
#         error, the io trhead should stop
#Details:
#      1) Fill in and setup variables
#      2) loop through looking for
#         sql threads to stop
#      3) If loops too long die.
####################################################
connection slave;
let $my_show= SHOW SLAVE STATUS;
let $sql_running= Slave_IO_Running;
let $row_number= 1;
let $run= 1;
let $counter= 300;

while ($run)
{
  let $io_result= query_get_value("SHOW SLAVE STATUS", Slave_IO_Running, $row_number);
  if (`SELECT '$io_result' = 'No'`){
    let $run= 0;
  }
  sleep 0.1;
  if (!$counter){
    --echo "Failed while waiting for slave IO thread to stop"
    query_vertical SHOW SLAVE STATUS;
    exit;
  }
  dec $counter;
}

