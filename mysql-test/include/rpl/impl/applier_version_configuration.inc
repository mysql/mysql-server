# ==== Purpose ====
#
# Configures replication option $rpl_applier_version from ENV variable
# $APPLIER_VERSION. This file is normally sourced by
# rpl/impl/change_topology_helper.inc
#
# ==== Usage ====
#
#  --source include/rpl/impl/applier_version_configuration.inc
#
# Parameters:
#
#  $skip_applier_version_configuration
#     If set, do not use environment variables to configure value of
#     APPLIER_VERSION
#
#
#  $rpl_applier_version
#     Include `APPLIER_VERSION` option in the `CHANGE REPLICATION SOURCE TO ...`
#     statement to be executed. For example, to specify that server_1 should
#     have the CSA enabled and server_5 shouldn't, do:
#
#       --let $rpl_applier_version= 1:2,5:1
#
#     To specify that all nodes have the CSA enabled, do:
#
#       --let $rpl_applier_version= *:2
#

--let $include_filename= rpl/impl/applier_version_configuration.inc
--source include/begin_include_file.inc

if ($skip_applier_version_configuration == '')
{
  # If parameter $rpl_applier_version is not instantiated by the calling test
  # script try to get its value from the environment variable
  if ($rpl_applier_version == '')
  {
    --let $rpl_applier_version= $RPL_CHANGE_STREAM_APPLIER
  }
}

--let $include_filename= rpl/impl/applier_version_configuration.inc
--source include/end_include_file.inc
