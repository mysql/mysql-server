# ==== Purpose ====
#
# Waits until the IO thread of the current connection has got an
# error, or until a timeout is reached. Also waits until the IO
# thread has completely stopped.
#
# ==== Usage ====
#
# source include/wait_for_slave_io_error.inc;
#
# Parameters:
#
# $slave_io_errno
#   The expected IO error number.  This is required.
#   (After BUG#41956 has been fixed, this will be required to be a
#   symbolic name instead of a number.)
#
# $show_slave_io_error
#   If set, will print the error to the query log.
# 
# $slave_timeout
#   See wait_for_slave_param.inc for description.
# 
# $master_connection
#   See wait_for_slave_param.inc for description.

if (`SELECT '$slave_io_errno' = ''`) {
  --die !!!ERROR IN TEST: you must set \$slave_io_errno before sourcing wait_for_slave_io_error.inc
}

let $slave_param= Slave_IO_Running;
let $slave_param_value= No;
let $slave_error_message= Failed while waiting for slave to stop the IO thread (expecting error in the IO thread);
source include/wait_for_slave_param.inc;

let $_error= query_get_value(SHOW SLAVE STATUS, Last_IO_Errno, 1);
if (`SELECT '$_error' != '$slave_io_errno'`) {
  --echo **** Slave stopped with wrong error code: $_error (expected $slave_io_errno) ****
  source include/show_rpl_debug_info.inc;
  --echo **** Slave stopped with wrong error code: $_error (expected $slave_io_errno) ****
  --die Slave stopped with wrong error code
}

if ($show_slave_io_error)
{
  let $error= query_get_value("SHOW SLAVE STATUS", Last_IO_Error, 1);
  echo Last_IO_Error = $error;
}
