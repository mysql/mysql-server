# ==== Purpose ====
#
# Waits until the IO thread of the current connection has stopped, or
# until a timeout is reached.
#
# ==== Usage ====
#
# source include/wait_for_slave_io_to_stop.inc;
#
# Parameters to this macro are $slave_timeout and
# $slave_keep_connection. See wait_for_slave_param.inc for
# descriptions.

let $slave_param= Slave_IO_Running;
let $slave_param_value= No;
let $slave_error_message= Failed while waiting for slave IO thread to stop;
source include/wait_for_slave_param.inc;
let $slave_error_message= ;
