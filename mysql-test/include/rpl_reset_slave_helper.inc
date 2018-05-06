# Auxiliary file used by rpl_reset.inc
# This file uses $rpl_channel_name set in rpl_for_each_connection.inc

if (!$rpl_reset_slave_all)
{
  if (!$rpl_multi_source)
  {
    RESET SLAVE;
  }
  if ($rpl_multi_source)
  {
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

