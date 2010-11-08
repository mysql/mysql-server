# ==== Purpose ====
#
# Waits until SHOW SLAVE STATUS has returned a specified value, or
# until a timeout is reached.
#
# ==== Usage ====
#
# let $slave_param= Slave_SQL_Running;
# let $slave_param_value= No;
# source include/slave_wait_param.inc;
#
# Parameters:
#
# $slave_param, $slave_param_value
#   This macro will wait until the column of the output of SHOW SLAVE
#   STATUS named $slave_param gets the value $slave_param_value.  See
#   the example above.
#
# $slave_param_comparison
#   By default, this file waits until $slave_param becomes equal to
#   $slave_param_value.  If you want to wait until $slave_param
#   becomes *unequal* to $slave_param_value, set this parameter to the
#   string '!=', like this:
#     let $slave_param_comparison= !=;
#
# $slave_timeout
#   The default timeout is 5 minutes. You can change the timeout by
#   setting $slave_timeout. The unit is tenths of seconds.
#
# $master_connection
#   If the timeout is reached, debug info is given by calling SHOW
#   SLAVE STATUS, SHOW PROCESSLIST, and SHOW BINLOG EVENTS.  Then, a
#   'connection master' is then issued, and more debug info is given
#   by calling SHOW MASTER STATUS, SHOW PROCESSLIST, and SHOW BINLOG
#   EVENTS.  If $master_connection is set, the latter three commands
#   will be issued on $master_connection instead of on the host named
#   'master'.  See also show_rpl_debug_info.inc
#
# $slave_error_message
#   If set, this is printed when a timeout occurs. This is primarily
#   intended to be used by other wait_for_slave_* macros, to indicate
#   what the purpose of the wait was. (A very similar error message is
#   given by default, but the wait_for_slave_* macros use this to give
#   an error message identical to that in previous versions, so that
#   errors are easier searchable in the pushbuild history.)

let $_slave_timeout_counter= $slave_timeout;
if (!$_slave_timeout_counter)
{
  let $_slave_timeout_counter= 3000;
}

let $_slave_param_comparison= $slave_param_comparison;
if (!$_slave_param_comparison)
{
  let $_slave_param_comparison= =;
}

let $_show_slave_status_value= query_get_value("SHOW SLAVE STATUS", $slave_param, 1);
while (`SELECT NOT('$_show_slave_status_value' $_slave_param_comparison '$slave_param_value') AND $_slave_timeout_counter > 0`)
{
  dec $_slave_timeout_counter;
  if ($_slave_timeout_counter)
  {
    sleep 0.1;
    let $_show_slave_status_value= query_get_value("SHOW SLAVE STATUS", $slave_param, 1);
  }
}

# This has to be outside the loop until BUG#41913 has been fixed
if (!$_slave_timeout_counter)
{
  --echo **** ERROR: timeout after $slave_timeout seconds while waiting for slave parameter $slave_param $_slave_param_comparison $slave_param_value ****
  if ($slave_error_message)
  {
    --echo Message: $slave_error_message
  }
  --echo Current connection is '$CURRENT_CONNECTION'
  echo Note: the following output may have changed since the failure was detected;
  source include/show_rpl_debug_info.inc;
  die;
}
