# ==== Purpose ====
#
# Waits until the IO thread of the current connection has started and
# connected to the master (i.e., until SHOW SLAVE STATUS returns Yes
# in the Slave_IO_Running field), or until a timeout is reached.
#
# ==== Usage ====
#
# source include/wait_for_slave_io_to_start.inc;
#
# Parameters to this macro are $slave_timeout and
# $master_connection. See wait_for_slave_param.inc for
# descriptions.

let $slave_param= Slave_IO_Running;
let $slave_param_value= Yes;
let $slave_error_message= Failed while waiting for slave IO thread to start;
source include/wait_for_slave_param.inc;
let $slave_error_message= ;
