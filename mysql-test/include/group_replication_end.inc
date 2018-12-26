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
# [--let $rpl_group_replication_reset_persistent_vars= 1]
# --source include/group_replication_end.inc
#
# Parameters:
#   $install_clean_up_required
#     See include/install_group_replication_plugin.inc
#
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
#   $rpl_group_replication_reset_persistent_vars
#     This instructs the script to use RESET PERSIST for
#     single primary variables set when you change mode for example
#

--let $include_filename= group_replication_end.inc
--source include/begin_include_file.inc

if ($install_clean_up_required)
{
  --source include/clean_group_replication_install_entry.inc
  --let $install_clean_up_required= 0
}

# Shut down Group Replication
--source include/rpl_end.inc

# Clean Group Replication XCom options on all servers to avoid false
# errors on MTR check testcase.
--source include/gr_clear_configuration.inc

# if running in single primary mode, reset to saved configs
if ($rpl_group_replication_single_primary_mode)
{
  --let $reset_single_primary_mode= 1
  --source include/gr_single_primary_mode.inc
}

if ($rpl_group_replication_reset_persistent_vars)
{
  --source include/gr_clear_persistent_variables.inc
}

--connection default
# Do not restore connection, because we have disconnected it.
--let $skip_restore_connection= 1
--let $include_filename= group_replication_end.inc
--source include/end_include_file.inc
