# ==== Purpose ====
#
# Shut down and shut up the server given by $rpl_server_number.  This
# is equivalent to rpl_stop_server.inc followed by rpl_start_server.inc
#
# ==== Usage ====
#
# --let $rpl_server_number= N
# [--let $rpl_start_with_gtids= [0|1]]
# [--let $rpl_server_parameters= --flag1 --flag2 ...]
# [--let $rpl_omit_print_server_parameters= 0]
# [--let $rpl_debug= 1]
# --source include/rpl_restart_server.inc
#
# Parameters:
#
#   $rpl_server_number, $rpl_start_with_gtids, $rpl_server_parameters,
#   $rpl_omit_print_server_parameters
#     See include/rpl_start_server.inc
#
#   $rpl_debug
#     See include/rpl_init.inc
#
# ==== See also ====
#
# rpl_start_server.inc
# rpl_stop_server.inc


--let $_rpl_restart_server_args= server_number=$rpl_server_number
if (!$rpl_omit_print_server_parameters) {
  if ($rpl_start_with_gtids == 1)
  {
    --let $_rpl_restart_server_args= $_rpl_restart_server_args gtids=on
  }
  if ($rpl_start_with_gtids == 0)
  {
    --let $_rpl_restart_server_args= $_rpl_restart_server_args gtids=off
  }
}

if ($rpl_server_parameters)
{
  if (!$rpl_omit_print_server_parameters)
  {
    --let $_rpl_restart_server_args= $_rpl_restart_server_args parameters: $rpl_server_parameters
  }
}

--let $include_filename= rpl_restart_server.inc [$_rpl_restart_server_args]

--source include/begin_include_file.inc

--source include/rpl_stop_server.inc
--source include/rpl_start_server.inc

--let $include_filename= rpl_restart_server.inc [$_rpl_restart_server_args]

--source include/end_include_file.inc
