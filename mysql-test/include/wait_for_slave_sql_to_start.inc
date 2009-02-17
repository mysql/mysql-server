# ==== Purpose ====
#
# Waits the SQL thread of the current connection has started, or until
# a timeout is reached.
#
# ==== Usage ====
#
# source include/wait_for_slave_sql_to_start.inc;
#
# Parameters to this macro are $slave_timeout and
# $master_connection. See wait_for_slave_param.inc for
# descriptions.

let $slave_param= Slave_SQL_Running;
let $slave_param_value= Yes;
let $slave_error_message= Failed while waiting for slave SQL to start;
source include/wait_for_slave_param.inc;
