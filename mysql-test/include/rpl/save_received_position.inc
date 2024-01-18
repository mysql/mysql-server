# ==== Purpose ====
#
# Save the current position of the IO thread, just like
# rpl/save_server_position.inc does for the master's position.
#
#
# ==== Usage ====
#
# [--let $use_gtids= 1]
# [--let $rpl_debug= 1]
# --source include/rpl/save_server_position.inc
#
# Typically, you would use this script together with
# include/rpl/sync_received_with_saved.inc
#
# Parameters:
#
#   $use_gtids
#     By default, this script saves the binlog file and offset. If
#     $use_gtids is set, this script saves the gtids.
#
#   $rpl_channel_name
#     Uses this channel name rather than the default channel.
#
#   $rpl_debug
#     See include/rpl/init.inc


--let $include_filename= rpl/save_received_position.inc
--let $_for_channel_clause=
if ($rpl_channel_name)
{
  --let $_for_channel_clause= FOR CHANNEL $rpl_channel_name
  --let $include_filename= $include_filename [FOR CHANNEL $rpl_channel_name]
}
--source include/begin_include_file.inc

if ($rpl_multi_source)
{
  if (!$rpl_channel_name)
  {
    --die ERROR IN TEST: when $rpl_multi_source is enabled, you must use $rpl_channel_name with rpl/save_received_pos.inc
  }
}

#
# Always save IO thread positions, this will be useful to check missing GTIDs.
#
--let $sss_statement= SHOW REPLICA STATUS $_for_channel_clause
--let $_saved_file= query_get_value($sss_statement, Source_Log_File, 1)
--let $_saved_pos= query_get_value($sss_statement, Read_Source_Log_Pos, 1)
--let $_saved_channel_name= $rpl_channel_name
if ($use_gtids)
{
  --source include/rpl/assert_gtid_mode_on.inc
  --let $_saved_gtids= query_get_value($sss_statement, Retrieved_Gtid_Set, 1)
  if ($rpl_debug)
  {
    --echo save_io_thread_pos saved gtid='$_saved_gtids', file='$_saved_file', pos='$_saved_pos'
  }
}
if (!$use_gtids)
{
  if ($rpl_debug)
  {
    --echo rpl/save_received_pos.inc saved file='$_saved_file', pos='$_saved_pos', channel_name='$_saved_channel_name'
  }
}

--let $include_filename= rpl/save_received_position.inc
--source include/end_include_file.inc
