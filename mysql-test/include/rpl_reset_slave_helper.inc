# Auxiliary file used by rpl_reset.inc
# This file uses $rpl_channel_name set in rpl_for_each_connection.inc

if (!$rpl_reset_slave_all)
{
  if (!$rpl_multi_source)
  {
    if ($rpl_privilege_checks_user)
    {
      CHANGE MASTER TO PRIVILEGE_CHECKS_USER=NULL;
    }
    if ($rpl_require_row_format)
    {
      CHANGE MASTER TO REQUIRE_ROW_FORMAT=0;
    }
    RESET SLAVE;
  }
  if ($rpl_multi_source)
  {
    if ($rpl_privilege_checks_user)
    {
      eval CHANGE MASTER TO PRIVILEGE_CHECKS_USER=NULL FOR CHANNEL $rpl_channel_name;
    }
    if ($rpl_require_row_format)
    {
      eval CHANGE MASTER TO REQUIRE_ROW_FORMAT=0 FOR CHANNEL $rpl_channel_name;
    }
    eval RESET SLAVE FOR CHANNEL $rpl_channel_name;
  }
}

if ($rpl_reset_slave_all)
{
  if (!$rpl_multi_source)
  {
    RESET SLAVE ALL;
  }
  if ($rpl_multi_source)
  {
    eval RESET SLAVE ALL FOR CHANNEL $rpl_channel_name;
  }
}
