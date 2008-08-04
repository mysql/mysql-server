# ==== Purpose ====
#
# Waits the SQL thread of the current connection has stopped, or until
# a timeout is reached.
#
# ==== Usage ====
#
# source include/wait_for_slave_sql_to_stop.inc;
#
# Parameters to this macro are $slave_timeout and
# $slave_keep_connection. See wait_for_slave_param.inc for
# descriptions.

let $slave_param= Slave_SQL_Running;
let $slave_param_value= No;
let $slave_error_message= Failed while waiting for slave SQL thread to stop;
source include/wait_for_slave_param.inc;
let $slave_error_message= ;
