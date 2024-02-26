# ==== Purpose ====
# Configures replication option $rpl_gtid_only from ENV variable $GTID_ONLY.
# This file is normally sourced by rpl_change_topology_helper.inc
#
# ==== Usage ====
#  --source include/rpl_gtid_only_configuration.inc
#
# Parameters:
#
#  $skip_configuration_rpl_gtid_only.inc
#     If set, do not use environment variables to configure value of GTID_ONLY
#
#
#  $rpl_gtid_only
#     Include `GTID_ONLY` option in the `CHANGE REPLICATION SOURCE TO ...`
#     statement to be executed. For example, to specify that server_1 should
#     have the flag enabled and server_5 shouldn't, do:
#
#       --let $rpl_gtid_only= 1:1,5:0
#
#     To specify that all nodes have the flag enabled, do:
#
#       --let $rpl_gtid_only= *:1
#

--let $include_filename= rpl_gtid_only_configuration.inc
--source include/begin_include_file.inc

if ($skip_configuration_rpl_gtid_only == '')
{
  # If parameter $rpl_gtid_only is not instantiated by the calling test script
  # try to get their values from environment variables
  if ($rpl_gtid_only == '')
  {
    --let $rpl_gtid_only= $GTID_ONLY
  }
}

--let $include_filename= rpl_gtid_only_configuration.inc
--source include/end_include_file.inc
