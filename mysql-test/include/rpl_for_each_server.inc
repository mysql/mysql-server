# ==== Purpose ====
#
# Source a .inc file once for server that was configured by
# rpl_init.inc.  For each server N, $rpl_server will be set to the N,
# and the current connection to server_N, and then the file will be
# sourced.
#
# ==== Usage ====
#
# --let $rpl_source_file= FILE
# [--let $rpl_debug= 1]
# --source include/rpl_for_each_server.inc
#
# Parameters:
#   $rpl_source_file
#     File that will be sourced.
#
#   $rpl_debug
#     See include/rpl_init.inc


if (!$rpl_source_file)
{
  --die !!!ERROR IN TEST: You must set $rpl_source_file
}

--let $include_filename= rpl_for_each_server.inc [$rpl_source_file]
--source include/begin_include_file.inc


--let $rpl_server= 1
while ($rpl_server <= $rpl_server_count)
{
  if ($rpl_debug)
  {
    --echo # debug: rpl_for_each_server.inc: server=$rpl_server
  }

  --let $rpl_connection_name= server_$rpl_server
  --source include/rpl_connection.inc

  --source $rpl_source_file

  --inc $rpl_server
}

--let $include_filename= rpl_for_each_server.inc [$rpl_source_file]
--source include/end_include_file.inc
