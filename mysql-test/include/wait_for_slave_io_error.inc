# ==== Purpose ====
#
# Waits until the IO thread of the current connection has got an
# error, or until a timeout is reached.
#
# ==== Usage ====
#
# source include/wait_for_slave_io_error.inc;
#
# Parameters to this macro are $slave_timeout and
# $slave_keep_connection. See wait_for_slave_param.inc for
# descriptions.

let $old_slave_param_comparison= $slave_param_comparison;

let $slave_param= Last_IO_Errno;
let $slave_param_comparison= !=;
let $slave_param_value= 0;
let $slave_error_message= Failed while waiting for slave to produce an error in its sql thread;
source include/wait_for_slave_param.inc;
let $slave_error_message= ;

let $slave_param_comparison= $old_slave_param_comparison;
