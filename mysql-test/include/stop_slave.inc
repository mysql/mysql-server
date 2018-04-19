# ==== Purpose ====
#
# Issues STOP SLAVE on the current connection. Then waits until both
# the IO and SQL threads have stopped, or until a timeout is reached.
#
# Please use this instead of 'STOP SLAVE', to reduce the risk of races
# in test cases.
#
# This will fail if the slave IO or SQL thread has an error. If you
# expect an error in the IO thread, use
# include/wait_for_slave_io_error.inc and include/stop_slave_sql.inc.
#
#
# ==== Usage ====
#
# [--let $rpl_only_running_threads= 1]
# [--let $rpl_channel_name= 'NAME']
# [--let $slave_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/stop_slave.inc
#
# Parameters:
#   $rpl_only_running_threads
#     By default, this script executes STOP SLAVE unconditionally.
#     This generates a warnings if one or both slave threads are
#     already stopped.  If $rpl_only_running_threads is set, this
#     script checks which slave threads are running, and issues either
#     STOP SLAVE, STOP SLAVE SQL_THREAD, STOP SLAVE IO_THREAD, or
#     nothing.
#
#   $rpl_channel_name
#     If set, a FOR CHANNEL $rpl_channel_name is added.
#
#   $slave_timeout
#     See include/wait_for_slave_param.inc
#
#   $rpl_debug
#     See include/rpl_init.inc


--let $include_filename= stop_slave.inc
--let $_for_channel_clause=
if ($rpl_channel_name)
{
  --let $_for_channel_clause= FOR CHANNEL $rpl_channel_name
  --let $include_filename= $include_filename [FOR CHANNEL $rpl_channel_name]
}
--source include/begin_include_file.inc


if (!$rpl_debug)
{
  --disable_query_log
}

--let $_for_channel_clause=
if ($rpl_channel_name)
{
  --let $_for_channel_clause= FOR CHANNEL $rpl_channel_name
}

if ($rpl_only_running_threads)
{
  --let $_slave_sql_running= query_get_value(SHOW SLAVE STATUS $_for_channel_clause, Slave_SQL_Running, 1)
  --let $_slave_io_running= query_get_value(SHOW SLAVE STATUS $_for_channel_clause, Slave_IO_Running, 1)
  if ($rpl_debug)
  {
    --echo Stop SQL: $_slave_sql_running; Stop IO: $_slave_io_running
  }

  --let $_slave_running_bits= `SELECT IF('$_slave_io_running' = 'Yes', 1, 0) + IF('$_slave_sql_running' = 'Yes', 2, 0)`
  if ($_slave_running_bits)
  {
    --dec $_slave_running_bits
    # $_slave_running_bits=1: io thread running
    if (!$_slave_running_bits)
    {
      --source include/stop_slave_io.inc
    }
    --dec $_slave_running_bits
    # $_slave_running_bits=2: sql thread running
    if (!$_slave_running_bits)
    {
      --source include/stop_slave_sql.inc
    }
    --dec $_slave_running_bits
    # $_slave_running_bits=2: both threads running
    if (!$_slave_running_bits)
    {
      eval STOP SLAVE $_for_channel_clause;
      --source include/wait_for_slave_to_stop.inc
    }
  }
}
if (!$rpl_only_running_threads)
{
  eval STOP SLAVE $_for_channel_clause;
  --source include/wait_for_slave_to_stop.inc
}


--let $include_filename= stop_slave.inc
--source include/end_include_file.inc
