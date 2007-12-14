###################################################
#Author: Sven
#Date: 2007-10-09
#Purpose: Wait until the slave has an error in the
#         sql thread, as indicated by
#         "SHOW SLAVE STATUS", or at most 30
#         seconds.
#Details:
#      1) Fill in and setup variables
#      2) loop, looking for sql error on slave
#      3) If it loops too long, die.
####################################################
connection slave;
let $row_number= 1;
let $run= 1;
let $counter= 300;

while ($run)
{
  let $sql_result= query_get_value("SHOW SLAVE STATUS", Last_SQL_Errno, $row_number);
  let $run= `SELECT '$sql_result' = '0'`;
  if ($run) {
    real_sleep 0.1;
    if (!$counter){
      --echo "Failed while waiting for slave to produce an error in its sql thread"
      --replace_result $MASTER_MYPORT MASTER_PORT
      --replace_column 1 # 7 # 8 # 9 # 22 # 23 # 33 #
      query_vertical SHOW SLAVE STATUS;
      exit;
    }
    dec $counter;
  }
}
