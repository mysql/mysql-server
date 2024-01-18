# ==== Purpose ====
#
# Reset the configuration of the replication server by calling
# rpl/impl/reset_replica_helper.inc..
# Used by tests which need to reset a single replication slave.
# Calling rpl/reset.inc would reset all the replication slaves.
#
#
# ==== Usage ====
#
# [--let $rpl_reset_slave_all= 1]
# [--let $rpl_multi_source= 1]
# [--let $rpl_privilege_checks_user=1]
# [--let $rpl_require_row_format= 1]
# --source include/rpl/reset_replica.inc
#
# Parameters:
#   $rpl_reset_slave_all
#     If set resets the slave using command "RESET REPLICA ALL".
#     Otherwise reset the slave using command "RESET REPLICA".
#   $rpl_multi_source
#     If set resets the channel with name $rpl_channel_name
#     Otherwise reset the default replication channel.
#   $rpl_privilege_checks_user
#     If set resets the default configuration of PRIVILEGE_CHECKS_USER.
#   $rpl_require_row_format
#     If set resets the default configuration of REQUIRE_ROW_FORMAT.

--let $include_filename= rpl/reset_replica.inc
--source include/begin_include_file.inc

if (!$rpl_debug)
{
  --disable_query_log
}

--source include/rpl/impl/reset_replica_helper.inc

if (!$rpl_debug)
{
  --enable_query_log
}

--let $include_filename= rpl/reset_replica.inc
--source include/end_include_file.inc
