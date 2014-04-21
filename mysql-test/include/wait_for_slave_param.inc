# ==== Purpose ====
#
# Waits until SHOW SLAVE STATUS has returned a specified value, or
# until a timeout is reached.
#
#
# ==== Usage ====
#
# --let $slave_param= Slave_SQL_Running
# --let $slave_param_value= No
# [--let $slave_param_comparison= [ < | <= | >= | > | = | != ]]
# [--let $slave_timeout= NUMBER]
# [--let $slave_error_param= [Slave_SQL_Errno | Slave_IO_Errno]]
# [--let $rpl_debug= 1]
# --source include/slave_wait_param.inc
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
#     --let $slave_param_comparison= !=
#
# $slave_timeout
#   The default timeout is 5 minutes. You can change the timeout by
#   setting $slave_timeout. The unit is seconds.
#
# $slave_error_param
#   If set, this script will check if the column of the output from
#   SHOW SLAVE STATUS named $slave_error_param is nonzero.  If it is,
#   this script will faile immediately.  Typically, this should be set
#   to Last_IO_Errno or Last_SQL_Errno.
#
# $rpl_debug
#   See include/rpl_init.inc


--let $include_filename= wait_for_slave_param.inc [$slave_param]
--source include/begin_include_file.inc


let $_slave_timeout= $slave_timeout;
if (!$_slave_timeout)
{
  let $_slave_timeout= 300;
}

if (`SELECT '$slave_error_param' = ''`)
{
  --let $slave_error_param= 1
}

let $_slave_param_comparison= $slave_param_comparison;
if (!$_slave_param_comparison)
{
  let $_slave_param_comparison= =;
}

if ($rpl_debug)
{
  --echo Waiting until '$slave_param' $_slave_param_comparison '$slave_param_value' [timeout='$_slave_timeout', \$slave_error_param='$slave_error_param']
}

--let $_slave_check_configured= query_get_value("SHOW SLAVE STATUS", Slave_IO_Running, 1)

if (`SELECT '$_slave_check_configured' = 'No such row'`)
{
  --echo **** ERROR: SHOW SLAVE STATUS returned empty result set. Slave not configured. ****
  --source include/show_rpl_debug_info.inc
  --die SHOW SLAVE STATUS returned empty result set. Slave not configured.
}

# mysqltest doesn't provide any better way to multiply by 10
--let $_wait_for_slave_param_zero= 0
--let $_slave_timeout_counter= $_slave_timeout$_wait_for_slave_param_zero
--let $_slave_continue= 1
while ($_slave_continue)
{
  --let $_show_slave_status_value= query_get_value("SHOW SLAVE STATUS", $slave_param, 1)

  # Check if an error condition is reached.
  if (!$slave_error_param)
  {
    --let $_show_slave_status_error_value= query_get_value("SHOW SLAVE STATUS", $slave_error_param, 1)
    if ($_show_slave_status_error_value)
    {
      --echo **** ERROR: $slave_error_param = '$_show_slave_status_error_value' while waiting for slave parameter $slave_param $_slave_param_comparison $slave_param_value ****
      --source include/show_rpl_debug_info.inc
      --die Error condition reached in include/wait_for_slave_param.inc
    }
  }

  # Check if the termination condition is reached.
  --let $_slave_continue= `SELECT NOT('$_show_slave_status_value' $_slave_param_comparison '$slave_param_value')`

  # Decrease timer, and check if the timeout is reached.
  if ($_slave_continue)
  {
    --dec $_slave_timeout_counter
    if (!$_slave_timeout_counter)
    {
      --echo **** ERROR: timeout after $_slave_timeout seconds while waiting for slave parameter $slave_param $_slave_param_comparison $slave_param_value ****
      --source include/show_rpl_debug_info.inc
      --die Timeout in include/wait_for_slave_param.inc
    }
    --sleep  0.1
  }
}


--let $include_filename= wait_for_slave_param.inc [$slave_param]
--source include/end_include_file.inc
