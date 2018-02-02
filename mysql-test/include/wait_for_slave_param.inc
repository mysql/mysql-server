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
# [--let $slave_error_param= [Last_SQL_Errno | Last_IO_Errno]]
# [--let $slave_error_param= [Slave_SQL_Errno | Slave_IO_Errno]]
# [--let $slave_io_running_check_disable= 1]
# [--let $rpl_channel_name= NAME]
# [--let $rpl_debug= 1]
# [--let $slave_io_errno= NUMBER [, NUMBER ...] [# comment]]
# [--let $slave_sql_errno= NUMBER [, NUMBER ...] [# comment]]
# --source include/wait_for_slave_param.inc
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
# $rpl_channel_name
#   If this is not '', adds a FOR CHANNEL  '$rpl_channel_name' clause
#   to SHOW SLAVE STATUS.
#
# $slave_timeout
#   The default timeout is 30 seconds. You can change the timeout by
#   setting $slave_timeout. The unit is one second.
#
# $slave_error_param
#   If set, this script will check for errors in the column of the
#   output from SHOW SLAVE STATUS named $slave_error_param while
#   waiting for the parameter. Once finding an error that is not
#   expected (see $slave_io_errno and $slave_sql_errno parameters)
#   this script will fail immediately. Typically, this should be set
#   to Last_IO_Errno or Last_SQL_Errno.
#
# $slave_io_running_check_disable
#   If set, this bypasses the check to see if the IO thread is started,
#   so params can be checked and verified for a stopped IO thread as
#   well. Otherwise check is made, and it is checked if the IO thread
#   is connected successfully with master.
#
# $rpl_debug
#   See include/rpl_init.inc
#
# $slave_io_errno
#   See include/wait_for_slave_io_error.inc
#
# $slave_sql_errno
#   See include/wait_for_slave_sql_error.inc
#
# ==== mysqltest variables configured by this file ====
#
# This file sets $slave_param_statement to the SQL statement used to
# get the slave status: either SHOW SLAVE STATUS or SHOW SLAVE STATUS
# FOR CHANNEL  '<channel>'.


--let $_wait_for_slave_params= $slave_param
if ($rpl_channel_name != '')
{
  --let $_wait_for_slave_params= $_wait_for_slave_params channel=$rpl_channel_name
}
--let $include_filename= wait_for_slave_param.inc [$_wait_for_slave_params]
--source include/begin_include_file.inc

--let $default_timeout= 30

--let $sleep_freq= 10
--let $sleep_time= `select 1.0 / $sleep_freq`

--let $start_to_wait=`select current_timestamp()`

let $_slave_timeout= $slave_timeout;
if (!$_slave_timeout)
{
  let $_slave_timeout= `select $default_timeout * $sleep_freq`;
}
if ($VALGRIND_TEST) {
  let $_slave_timeout= `select $_slave_timeout * 6`;
}

if ($slave_error_param)
{
  if ($slave_error_param != "Last_SQL_Errno")
  {
    if ($slave_error_param != "Last_IO_Errno")
    {
      --echo *** slave_error_param = $slave_error_param
      --die slave_error_param must be null, Last_SQL_Errno or Last_IO_Errno
    }
  }
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

--let $slave_param_statement= SHOW SLAVE STATUS
if ($rpl_channel_name != '')
{
  --let $slave_param_statement= $slave_param_statement FOR CHANNEL  '$rpl_channel_name'
}

if (!$slave_io_running_check_disable)
{
  --let $_slave_check_configured= query_get_value($slave_param_statement, Slave_IO_Running, 1)

  if ($_slave_check_configured == 'No such row')
  {
    --echo **** ERROR: $slave_param_statement returned empty result set. Slave not configured. ****
    --source include/show_rpl_debug_info.inc
    --die SHOW SLAVE STATUS returned empty result set. Slave not configured.
  }
}


# Strip away comments on $slave_io_errno and $slave_sql_errno parameters
--let $_slave_io_errno= `SELECT IF(LOCATE('#', '$slave_io_errno') != 0, SUBSTR('$slave_io_errno', 1, LOCATE('#', '$slave_io_errno') - 1), '$slave_io_errno')`
--let $_slave_sql_errno= `SELECT IF(LOCATE('#', '$slave_sql_errno') != 0, SUBSTR('$slave_sql_errno', 1, LOCATE('#', '$slave_sql_errno') - 1), '$slave_sql_errno')`

--let $_slave_timeout_counter= `select $_slave_timeout * $sleep_freq`
--let $_slave_continue= 1
while ($_slave_continue)
{
  --let $_show_slave_status_value= query_get_value($slave_param_statement, $slave_param, 1)

  # Check if an error condition is reached.
  if ($slave_error_param)
  {
    --let $_show_slave_status_error_value= query_get_value($slave_param_statement, $slave_error_param, 1)

    # Check if the error condition was expected
    if ($_show_slave_status_error_value)
    {
      --let $_expected_error=
      if ($slave_error_param == "Last_IO_Errno")
      {
        --let $_expected_error=`SELECT FIND_IN_SET('$_show_slave_status_error_value','$_slave_io_errno')`
      }
      if ($slave_error_param == "Last_SQL_Errno")
      {
        --let $_expected_error=`SELECT FIND_IN_SET('$_show_slave_status_error_value','$_slave_sql_errno')`
      }
      # If the error is an expected error, just ignore it
      if ($_expected_error)
      {
        --let $_show_slave_status_error_value=
      }
    }

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
      --let $end_to_wait=`select current_timestamp()`

      --echo **** ERROR: timeout after $_slave_timeout ($end_to_wait - $start_to_wait) seconds while waiting for slave parameter $slave_param $_slave_param_comparison $slave_param_value ****
      --source include/show_rpl_debug_info.inc
      --die Timeout in include/wait_for_slave_param.inc
    }
    --sleep  $sleep_time
  }
}


--let $include_filename= wait_for_slave_param.inc [$slave_param]
--source include/end_include_file.inc
