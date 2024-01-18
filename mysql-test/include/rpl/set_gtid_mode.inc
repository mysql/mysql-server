# ==== Purpose ====
#
# Switch GTID_MODE on all servers or on a subset of them.  Step on all
# servers simultaneously, so that replication does not need to stop.
#
# Optionally, sync replication first.
#
# This works even if rpl/init.inc has not been invoked; in that case
# it assumes there is only one server running.
#
# ==== Usage ====
#
# [--let $rpl_gtid_mode= [OFF|OFF_PERMISSIVE|ON_PERMISSIVE|ON|0|1|2|3]]
# [--let $rpl_set_enforce_gtid_consistency= [0|1]]
# [--let $rpl_server_numbers= [X[,Y[,Z...]]]
# [--let $rpl_skip_sync= 1]
# [--let $rpl_only_running_threads= 1]
# [--let $rpl_debug= 1]
# [--let $no_extra_info= 0]
# --source include/rpl/set_gtid_mode.inc
#
# Parameters:
#   $rpl_gtid_mode= [OFF|OFF_PERMISSIVE|ON_PERMISSIVE|ON|0|1|2|3]
#     By default, switches to ON. Set this to switch to any other mode.
#
#   $rpl_set_enforce_gtid_consistency
#     By default, leaves ENFORCE_GTID_CONSISTENCY unchagned. If this is
#     set, changes ENFORCE_GTID_CONSISTENCY to the specified value.
#
#   $rpl_server_numbers= [X[,Y[,Z...]]]
#     By default, switches GTID_MODE on all servers configured by
#     rpl_init.  Set $rpl_server_numbers to a comma-separated list if
#     you want to switch on a specific subset of the servers.
#
#   $rpl_skip_sync
#     By default, all slaves are synced using rpl/sync.inc. Set this
#     option to 1 to disable this behavior (note that you must
#     manually sync all servers in this case). Normally you want to
#     sync, but you need to disable sync if you use multi-source.
#
#   $rpl_only_running_threads
#     See include/rpl/sync.inc
#
#   $rpl_debug=1
#     See include/rpl/init.inc
#
#   $no_extra_info=0
#     Do not print the mode and server after the include file name. This option
#     allows to easly support tests that may invoke with dynamic parameter
#     values.
#

# Get the new gtid_mode
--let $_rsgm_new_mode= $rpl_gtid_mode
if ($_rsgm_new_mode == '')
{
  --let $_rsgm_new_mode= ON
}

# Get list of servers
--let $_rsgm_server_list= $rpl_server_numbers
if ($_rsgm_server_list == '')
{
  if ($rpl_inited)
  {
    --let $_rsgm_server_count= $_rpl_server_count
    if ($_rsgm_server_count == '')
    {
      --die !!!ERROR IN TEST FRAMEWORK: $_rpl_server_count should be set if $rpl_inited is set
    }
    --let $_rsgm_server_list= 1
    --let $_rsgm_server_number= 2
    while ($_rsgm_server_number <= $_rsgm_server_count)
    {
      --let $_rsgm_server_list= $_rsgm_server_list,$_rsgm_server_number
      --inc $_rsgm_server_number
    }
  }
  if (!$rpl_inited)
  {
    --let $_rsgm_server_list= 1
  }
}

--let $_rsgm_mute= $no_extra_info
if ($_rsgm_mute == '')
{
  --let $_rsgm_mute= 0
}

--let $include_filename= rpl/set_gtid_mode.inc
if ($_rsgm_mute == 0)
{
  --let $include_filename= rpl/set_gtid_mode.inc [$_rsgm_new_mode on servers $_rsgm_server_list]
}
--source include/begin_include_file.inc

if ($rpl_inited)
{
  if (!$rpl_skip_sync)
  {
    --source include/rpl/sync.inc
  }

  --let $_rsgm_server_number= `SELECT SUBSTRING_INDEX('$_rsgm_server_list', ',', 1)`
  --connection server_$_rsgm_server_number
}
--let $_rsgm_current_mode= `SELECT FIND_IN_SET(@@GLOBAL.GTID_MODE, 'OFF,OFF_PERMISSIVE,ON_PERMISSIVE,ON')`
--dec $_rsgm_current_mode

--let $_rsgm_new_mode= `SELECT FIND_IN_SET('$_rsgm_new_mode', 'OFF,OFF_PERMISSIVE,ON_PERMISSIVE,ON') + FIND_IN_SET('$_rsgm_new_mode', '0,1,2,3')`
--dec $_rsgm_new_mode
if ($_rsgm_new_mode == -1)
{
  --echo !!!ERROR IN TEST: rpl_gtid_mode was set to '$rpl_gtid_mode'
  --die !!!ERROR IN TEST: Wrong value for $rpl_gtid_mode. Set one of the valid values.
}

if (!$rpl_debug)
{
  --disable_query_log
}
if ($rpl_debug)
{
  --echo OLD:$_rsgm_current_mode NEW:$_rsgm_new_mode
}

if ($rpl_set_enforce_gtid_consistency == 1)
{
  --let $_rsgm_server_todo= $_rsgm_server_list

  while ($_rsgm_server_todo)
  {
    if ($rpl_inited)
    {
      --let $_rsgm_server_number= `SELECT SUBSTRING_INDEX('$_rsgm_server_todo', ',', 1)`

      --let $rpl_connection_name= server_$_rsgm_server_number
      --source include/connection.inc
    }
    eval SET @@GLOBAL.ENFORCE_GTID_CONSISTENCY= 1;

    --let $_rsgm_server_todo= `SELECT SUBSTRING('$_rsgm_server_todo', LENGTH('$_rsgm_server_number') + 2)`
  }
}

while ($_rsgm_current_mode != $_rsgm_new_mode)
{
  if ($rpl_debug)
  {
    --echo current mode: $_rsgm_current_mode; new mode: $_rsgm_new_mode
  }
  if ($_rsgm_current_mode < $_rsgm_new_mode)
  {
    --inc $_rsgm_current_mode
  }
  if ($_rsgm_current_mode > $_rsgm_new_mode)
  {
    --dec $_rsgm_current_mode
  }

  --let $_rsgm_server_todo= $_rsgm_server_list

  while ($_rsgm_server_todo)
  {
    if ($rpl_inited)
    {
      --let $_rsgm_server_number= `SELECT SUBSTRING_INDEX('$_rsgm_server_todo', ',', 1)`

      --let $rpl_connection_name= server_$_rsgm_server_number
      --source include/connection.inc
    }
    eval SET @@GLOBAL.GTID_MODE= $_rsgm_current_mode;

    --let $_rsgm_server_todo= `SELECT SUBSTRING('$_rsgm_server_todo', LENGTH('$_rsgm_server_number') + 2)`
  }
}

if ($rpl_set_enforce_gtid_consistency == 0)
{
  --let $_rsgm_server_todo= $_rsgm_server_list

  while ($_rsgm_server_todo)
  {
    if ($rpl_inited)
    {
      --let $_rsgm_server_number= `SELECT SUBSTRING_INDEX('$_rsgm_server_todo', ',', 1)`

      --let $rpl_connection_name= server_$_rsgm_server_number
      --source include/connection.inc
    }
    eval SET @@GLOBAL.ENFORCE_GTID_CONSISTENCY= 0;

    --let $_rsgm_server_todo= `SELECT SUBSTRING('$_rsgm_server_todo', LENGTH('$_rsgm_server_number') + 2)`
  }
}

--let $include_filename= rpl/set_gtid_mode.inc
--source include/end_include_file.inc
