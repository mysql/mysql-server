# ==== Purpose ====
#
# Execute a .inc file once for server that was configured by
# rpl_init.inc.  For each server, $rpl_server will be set to the
# number of the server, and the current connection will be set to
# server_N, where N == $rpl_server.
#
# ==== Usage ====
#
# --let $rpl_source_file= FILE
# [--let $rpl_debug= 1]
# --source include/rpl_for_each_server.inc
#
# Parameters:
#   $rpl_source_file
#     The file that will be sourced.
#
#   $rpl_debug
#     See include/rpl_init.inc


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
