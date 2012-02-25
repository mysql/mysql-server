# ==== Purpose ====
#
# Sync all servers, stop all slaves, restart all servers with
# GTID_MODE=ON and start the slaves up again.
#
# This is a temporary solution until we make GTID_MODE dynamic.
#
# ==== Usage ====
#
# First, make sure you understand the restrictions implied by turning
# on GTIDs. In particular, myisam updates are not supported so if you
# need to call mtr.add_suppression you should do it before sourcing
# this file.  Then do the following (normally you don't need to use
# any of the parameters).
#
# [--let $rpl_gtid_mode_off= 1]
# [--let $rpl_server_parameters= --arg ...]
# [--let $rpl_skip_sync= 1]
# [--let $rpl_skip_stop_slave= 1]
# [--let $rpl_skip_stop_server= 1]
# [--let $rpl_skip_start_slave= 1]
# [--let $rpl_only_running_threads= 1]
# [--let $rpl_debug= 1]
# --source include/rpl_set_gtid_mode.inc
#
# Parameters:
#   $rpl_gtid_mode_off= 1
#     Starts the server with gtid_mode=OFF, not ON.
#
#   $rpl_server_parameters
#     Any extra --server-parameters you want to pass to mysqld.
#
#   $rpl_skip_stop_server
#     By default, this script stops the server, assuming it is
#     running.  If the server is already stopped, set
#     $rpl_no_stop_server=1 to inhibit this step.
#
#   $rpl_skip_sync
#     Do not sync the slaves before stopping the slave.
#
#   $rpl_only_running_threads
#     See include/rpl_sync.inc
#
#   $rpl_skip_stop_slave
#     Do not stop the slaves before stopping the server.
#
#   $rpl_skip_start_slave
#     Do not start the slaves again after starting the server.
#
#   $rpl_debug=1
#     See include/rpl_init.inc

--let $include_filename= rpl_set_gtid_mode.inc
--source include/begin_include_file.inc

if (!$rpl_skip_sync)
{
  --source include/rpl_sync.inc
}
if (!$rpl_skip_stop_slave)
{
  --source include/rpl_stop_slaves.inc
}

--let $rpl_start_with_gtids= 1
if ($rpl_gtid_mode_off)
{
  --let $rpl_start_with_gtids= 0
}

--let $rpl_server_number= 1
while ($rpl_server_number <= $rpl_server_count)
{
  if (!$rpl_skip_stop_server)
  {
    --source include/rpl_stop_server.inc
  }
  --source include/rpl_start_server.inc
  --inc $rpl_server_number
}

if (!$rpl_skip_start_slave)
{
  --source include/rpl_start_slaves.inc
}

--let $include_filename= rpl_set_gtid_mode.inc
--source include/end_include_file.inc
