# ==== Purpose ====
#
# Waits until the SQL thread of the current connection has got an
# error, or until a timeout is reached. Also waits until the SQL
# thread has completely stopped.
#
# ==== Usage ====
#
# # Wait several errors.  
# let $slave_sql_errno= 1, 2, 3; 
# source include/wait_for_slave_sql_error.inc;
#
# # Print error message 
# let $slave_sql_errno= 1; 
# let $show_slave_sql_error= 1; 
# source include/wait_for_slave_sql_error.inc;
#
# Parameters:
#
# $slave_sql_errno
#   The expected SQL error numbers.  This is required.
#   (After BUG#41956 has been fixed, this will be required to be a
#   symbolic name instead of a numbers.)
#
# $show_slave_sql_error
#   If set, will print the error to the query log.
# 
# $slave_timeout
#   See wait_for_slave_param.inc for description.
# 
# $master_connection
#   See wait_for_slave_param.inc for description.

if (!$slave_sql_errno) {
  --die !!!ERROR IN TEST: you must set \$slave_sql_errno before sourcing wait_for_slave_sql_error.inc
}

let $slave_param= Slave_SQL_Running;
let $slave_param_value= No;
let $slave_error_message= Failed while waiting for slave to stop the SQL thread (expecting error in the SQL thread);
source include/wait_for_slave_param.inc;

let $_error= query_get_value(SHOW SLAVE STATUS, Last_SQL_Errno, 1);
if (`SELECT $_error NOT IN ($slave_sql_errno)`) {
  --echo **** Slave stopped with wrong error code: $_error (expected $slave_sql_errno) ****
  source include/show_rpl_debug_info.inc;
  --echo **** Slave stopped with wrong error code: $_error (expected $slave_sql_errno) ****
  --die Slave stopped with wrong error code
}

if ($show_slave_sql_error)
{
  --let $error= query_get_value("SHOW SLAVE STATUS", Last_SQL_Error, 1)
  --let $error= `SELECT SUBSTR("$error", 17)`
  --replace_result $MYSQL_TEST_DIR MYSQL_TEST_DIR
  --replace_regex /[0-9]{6} [0-9]{2}:[0-9]{2}:[0-9]{2} //
  --echo Last_SQL_Error = $error
}
