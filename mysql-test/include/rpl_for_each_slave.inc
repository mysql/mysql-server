# ==== Purpose ====
#
# Execute a .inc file once for each server that was configured as a
# slave by rpl_init.inc
#
#
# ==== Usage ====
#
# --let $rpl_source_file
# [--let $rpl_debug= 1]
# --source include/rpl_for_each_slave.inc
#
# Parameters:
#   $rpl_source_file
#     The file that will be sourced.
#
#   $rpl_debug
#     See include/rpl_init.inc


--let $include_filename= rpl_for_each_file.inc [$rpl_source_file]
--source include/begin_include_file.inc

--let $_rpl_server= $rpl_server_count
while ($_rpl_server)
{
  --let $_rpl_has_master= `SELECT SUBSTRING('$rpl_master_list', 1 + ($_rpl_server - 1) * $rpl_server_count_length, $rpl_server_count_length) != ''`
  if ($_rpl_has_master)
  {
    --let $rpl_connection_name= server_$_rpl_server
    --source include/rpl_connection.inc
    --source $rpl_source_file
  }
  --dec $_rpl_server
}


--let $include_filename= rpl_for_each_file.inc [$rpl_source_file]
--source include/end_include_file.inc
