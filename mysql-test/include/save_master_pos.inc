# ==== Purpose ====
#
# Save the current binlog position on the master, just like the
# built-in mysqltest command save_master_pos.  The advantage of this
# script is that the saved position is available to the test script.
#
#
# ==== Usage ====
#
# [--let $use_gtids= 1]
# [--let $rpl_debug= 1]
# [--let $force_empty_channel_name= 1]
# --source include/save_master_pos.inc
#
# Typically, you would use this script together with
# include/sync_io_with_master.inc
#
# Parameters:
#
#   $use_gtids
#     By default, this script saves the binlog file and offset. If
#     $use_gtids is set, this script saves the gtids.
#
#   $rpl_debug
#     See include/rpl_init.inc
#
#   $force_empty_channel_name
#     Set it to 1 if you want to explicitly say that channel_name is
#     empty channel name (equal to '') and not unspecified.
#

--let $include_filename= save_master_pos.inc
--source include/begin_include_file.inc

#
# Always save master positions, this will be useful to check missing GTIDs.
#
--let $_saved_file= query_get_value(SHOW MASTER STATUS, File, 1)
--let $_saved_pos= query_get_value(SHOW MASTER STATUS, Position, 1)
--let $_saved_channel_name= $rpl_channel_name
if ($rpl_multi_source)
{
  if (!$rpl_channel_name)
  {
    if (!$force_empty_channel_name)
    {
      --let $_saved_channel_name= `SELECT CONCAT('channel_', @@GLOBAL.SERVER_ID)`
    }
  }
}
if ($use_gtids)
{
  --source include/assert_gtid_mode_on.inc
  --let $_saved_gtids= `SELECT @@global.gtid_executed`
  if ($rpl_debug)
  {
    --echo save_master_pos saved gtid='$_saved_gtids', file='$_saved_file', pos='$_saved_pos' channel_name='$_saved_channel_name'
  }
}
if (!$use_gtids)
{
  if ($rpl_debug)
  {
    --echo save_master_pos saved file='$_saved_file', pos='$_saved_pos' channel_name='$_saved_channel_name'
  }
}

--let $include_filename= save_master_pos.inc
--source include/end_include_file.inc
