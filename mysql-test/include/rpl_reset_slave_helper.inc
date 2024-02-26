# Auxiliary file used by rpl_reset.inc
# This file uses $rpl_channel_name set in rpl_for_each_connection.inc

if (!$rpl_reset_slave_all)
{
  if (!$rpl_multi_source)
  {
    if ($rpl_privilege_checks_user)
    {
      CHANGE REPLICATION SOURCE TO PRIVILEGE_CHECKS_USER=NULL;
    }
    if ($rpl_gtid_only)
    {
      CHANGE REPLICATION SOURCE TO GTID_ONLY=0;
    }
    if ($rpl_require_row_format)
    {
      CHANGE REPLICATION SOURCE TO REQUIRE_ROW_FORMAT=0;
    }
    RESET REPLICA;
  }
  if ($rpl_multi_source)
  {
    if ($rpl_privilege_checks_user)
    {
      eval CHANGE REPLICATION SOURCE TO PRIVILEGE_CHECKS_USER=NULL FOR CHANNEL $rpl_channel_name;
    }
    if ($rpl_gtid_only)
    {
      eval CHANGE REPLICATION SOURCE TO GTID_ONLY=0 FOR CHANNEL $rpl_channel_name;
    }
    if ($rpl_require_row_format)
    {
      eval CHANGE REPLICATION SOURCE TO REQUIRE_ROW_FORMAT=0 FOR CHANNEL $rpl_channel_name;
    }
    eval RESET REPLICA FOR CHANNEL $rpl_channel_name;
  }
}

if ($rpl_reset_slave_all)
{
  if (!$rpl_multi_source)
  {
    RESET REPLICA ALL;
  }
  if ($rpl_multi_source)
  {
    eval RESET REPLICA ALL FOR CHANNEL $rpl_channel_name;
  }
}
