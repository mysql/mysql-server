# ==== Purpose ====
#
# Waits until the SQL thread of the current connection has got an
# error, or until a timeout is reached. Also waits until the SQL
# thread has completely stopped.
#
# ==== Usage ====
#
# source include/wait_for_slave_sql_error.inc;
#
# Parameters:
#
# $slave_sql_errno
#   The expected SQL error number.  This is required.
#   (After BUG#41956 has been fixed, this will be required to be a
#   symbolic name instead of a number.)
# 
# $slave_timeout
#   See wait_for_slave_param.inc for description.
# 
# $master_connection
#   See wait_for_slave_param.inc for description.

if (`SELECT '$slave_sql_errno' = ''`) {
  --echo !!!ERROR IN TEST: you must set \$slave_sql_errno before sourcing wait_fro_slave_sql_error.inc
  exit;
}

let $slave_param= Slave_SQL_Running;
let $slave_param_value= No;
let $slave_error_message= Failed while waiting for slave to stop the SQL thread (expecting error in the SQL thread);
source include/wait_for_slave_param.inc;

let $_error= query_get_value(SHOW SLAVE STATUS, Last_SQL_Errno, 1);
if (`SELECT '$_error' != '$slave_sql_errno'`) {
  --echo Slave stopped with wrong error code: $_error (expected $slave_sql_errno)
  source include/show_rpl_debug_info.inc;
  exit;
}
