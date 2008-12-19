# ==== Purpose ====
#
# Waits until the SQL thread of the current connection has got an
# error, or until a timeout is reached.
#
# ==== Usage ====
#
# source include/wait_for_slave_sql_error.inc;
#
# Parameters:
#
# $slave_sql_errno
#   Number of expected SQL error. If it skipped then any error
#   will pass.
# 
# $slave_timeout and
#   See wait_for_slave_param.inc for descriptions.
# 
# $slave_keep_connection. 
#   See wait_for_slave_param.inc for descriptions.

let $old_slave_param_comparison= $slave_param_comparison;

let $slave_param= Last_SQL_Errno;
let $slave_param_comparison= !=;
let $slave_param_value= 0;

if ($slave_sql_errno) {
  let $slave_param_comparison= =;
  let $slave_param_value= $slave_sql_errno;
}

let $slave_error_message= Failed while waiting for slave to produce an error in its sql thread;
source include/wait_for_slave_param.inc;
let $slave_error_message= ;

let $slave_param_comparison= $old_slave_param_comparison;
