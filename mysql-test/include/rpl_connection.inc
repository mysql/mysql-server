# ==== Purpose ====
#
# The same as 'connection $rpl_connection_name', but it can also
# prints the connection name.  The connection is printed if $rpl_debug
# is set, or if rpl_connection.inc is not called between two
# invocations of begin_include_file.inc/end_include_file.inc.
# Otherwise the connection name is not printed.
#
#
# ==== Usage ====
#
# --let $rpl_connection_name= master
# [--let $rpl_connection_silent= 1]
# [--let $rpl_debug= 1]
# --source include/rpl_connection.inc
#
# Parameters:
#   $rpl_connection_name
#     Name of the connection to connect to.
#
#   $rpl_connection_silent
#     Do not print the connection name unless $rpl_debug is set.
#
#   $rpl_debug
#     By default, the connection name is printed only when this file
#     is sourced from a top-level test script. If $rpl_debug is set,
#     the connection name is also printed whenever auxiliary files
#     like rpl_init.inc change connection.


if (!$rpl_connection_name)
{
  --die ERROR IN TEST: you must set $rpl_connection_name before sourcing rpl_connection.inc
}

# This is the same as "if (!$_rpl_include_file_depth || $rpl_debug)",
# but the mysqltest language doesn't have boolean operations.

if ($rpl_debug)
{
  --echo [connection $rpl_connection_name]
}
if (!$rpl_debug)
{
  if (!$_include_file_depth)
  {
    if (!$rpl_connection_silent)
    {
      --echo [connection $rpl_connection_name]
    }
  }
}

--connection $rpl_connection_name
--let $rpl_connection_name=
