# ==== Purpose ====
#
# Waits until both the IO and SQL threads of the current connection
# have stopped, or until a timeout is reached.
#
# ==== Usage ====
#
# source include/wait_for_slave_to_stop.inc;
#
# Parameters to this macro are $slave_timeout and
# $slave_keep_connection. See wait_for_slave_param.inc for
# descriptions.

let $slave_error_message= Failed while waiting for slave to stop;

let $slave_param= Slave_IO_Running;
let $slave_param_value= No;
source include/wait_for_slave_param.inc;

let $slave_param= Slave_SQL_Running;
let $slave_param_value= No;
source include/wait_for_slave_param.inc;

let $slave_error_message= ;
