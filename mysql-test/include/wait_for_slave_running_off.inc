###################################################
#Author: Jeb
#Date: 2007-08-09
#Purpose: This is to address timing issue recorded  
#         in bug 30209. This function with wait
#         until "SHOW STATUS LIKE 'Slave_running'"
#         returns 'OFF'
#Details:
#      1) Fill in and setup variable
#      2) loop through looking OFF
#      3) If loops too long die.
####################################################
connection slave;
let $row_number= 1;
let $run= 1;
let $counter= 300;

while ($run)
{
  let $running_result= query_get_value(SHOW STATUS LIKE 'Slave_running', Value, $row_number);
  if (`SELECT '$running_result' = 'OFF'`){
    let $run= 0;
  }
  sleep 0.1;
  if (!$counter){
    --echo "Failed while waiting for slave to stop"
    --replace_result $MASTER_MYPORT MASTER_PORT
    --replace_column 1 # 7 # 8 # 9 # 22 # 23 # 33 #
    query_vertical SHOW SLAVE STATUS;
    exit;
  }
  dec $counter;
}


