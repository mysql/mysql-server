# ==== Purpose ====
#
# The same as 'connection $rpl_connection_name', but it can also
# prints the connection name.  The connection is printed if $rpl_debug
# is set, or if rpl/connection.inc is not called between two
# invocations of begin_include_file.inc/end_include_file.inc.
# Otherwise the connection name is not printed.
#
#
# ==== Usage ====
#
# --let $rpl_connection_name= master
# [--let $rpl_connection_silent= 1]
# [--let $rpl_debug= 1]
# --source include/connection.inc
#
# Parameters:
#   $rpl_connection_name
#     Name of the connection to connect to.
#
#   $rpl_connection_silent
#     Only print the connection name if $rpl_debug is set.
#
#   $rpl_connection_silent_if_same
#     Only print the connection name if it differs from the previous
#     one, or if $rpl_debug is set.
#
#   $rpl_debug
#     By default, the connection name is printed only when this file
#     is sourced from a top-level test script. If $rpl_debug is set,
#     the connection name is also printed whenever auxiliary files
#     like rpl/init.inc change connection.


if (!$rpl_connection_name)
{
  --die ERROR IN TEST: you must set $rpl_connection_name before sourcing rpl/connection.inc
}

# Print connection name if $rpl_debug is set.
# Otherwise, if we are outside any include file: if silent don't
# print; if silent_if_same and connection does not change don't print;
# otherwise print.
--let $_rc_print_connection_name= 0
if (!$_include_file_depth)
{
  --let $_rc_print_connection_name= 1
  if ($rpl_connection_silent_if_same)
  {
    if ($rpl_connection_name == $CURRENT_CONNECTION)
    {
      --let $_rc_print_connection_name= 0
    }
  }
  if ($rpl_connection_silent)
  {
    --let $_rc_print_connection_name= 0
  }
}
if ($rpl_debug)
{
  --let $_rc_print_connection_name= 1
}
if ($_rc_print_connection_name)
{
  --echo [connection $rpl_connection_name]
}

--connection $rpl_connection_name
--let $rpl_connection_name=
