# ==== Purpose ====
#
# Execute an SQL statement, once for server that was configured by
# rpl/init.inc, on connection server_N.
#
# ==== Usage ====
#
# --let $rpl_sql= STATEMENT
# [--let $rpl_debug= 1]
# --source include/rpl/for_each_server_stmt.inc
#
# Parameters:
#   $rpl_sql
#     SQL that will be executed.
#
#   $rpl_debug
#     See include/rpl/init.inc


if (!$rpl_sql)
{
  --die !!!ERROR IN TEST: You must set $rpl_sql
}

--let $include_filename= rpl/for_each_server_stmt.inc [$rpl_sql]
--source include/begin_include_file.inc


--let $rpl_server= 1
while ($rpl_server <= $rpl_server_count)
{
  if ($rpl_debug)
  {
    --echo # debug: rpl/for_each_server.inc: server=$rpl_server
  }

  --let $rpl_connection_name= server_$rpl_server
  --source include/connection.inc

  --eval $rpl_sql

  --inc $rpl_server
}


--let $include_filename= rpl/for_each_server_stmt.inc [$rpl_sql]
--source include/end_include_file.inc
