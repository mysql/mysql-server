# ==== Purpose ====
#
# Shut down Group Replication initialized by
# include/group_replication.inc.
#
# ==== Usage ====
#
# [--let $rpl_debug= 1]
# [--let $rpl_group_replication= 1]
# [--let $rpl_group_replication_single_primary_mode= 1]
# --source include/group_replication_end.inc
#
# Parameters:
#   $rpl_debug
#     See include/rpl_init.inc
#
#   $rpl_group_replication
#     See include/rpl_end.inc
#
#   $rpl_group_replication_single_primary_mode
#     This instructs this script to run the script that
#     resets the single primary mode configs
#

--let $include_filename= group_replication_end.inc
--source include/begin_include_file.inc

# Shut down Group Replication
--source include/rpl_end.inc

# Clean Group Replication XCom options on all servers to avoid false
# errors on MTR check testcase.
--source ../inc/gr_clear_configuration.inc

# if running in single primary mode, reset to saved configs
if ($rpl_group_replication_single_primary_mode)
{
  --let $reset_single_primary_mode= 1
  --source ../inc/gr_single_primary_mode.inc
}

--connection default
# Do not restore connection, because we have disconnected it.
--let $skip_restore_connection= 1
--let $include_filename= group_replication_end.inc
--source include/end_include_file.inc
