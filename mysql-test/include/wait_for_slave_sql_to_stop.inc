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
# $master_connection. See wait_for_slave_param.inc for
# descriptions.

# if server has not used CHANGE MASTER to initiate slave, SHOW SLAVE
# STATUS will return an empty set.
let $_slave_io_running= query_get_value("SHOW SLAVE STATUS", Slave_IO_Running, 1);
if (`SELECT '$_slave_io_running' != 'No such row'`)
{
  let $slave_param= Slave_SQL_Running;
  let $slave_param_value= No;
  let $slave_error_message= Failed while waiting for slave SQL thread to stop;
  source include/wait_for_slave_param.inc;
  let $slave_error_message= ;
}
