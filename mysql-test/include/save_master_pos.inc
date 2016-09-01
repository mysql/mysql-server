# ==== Purpose ====
#
# Save the current binlog position on the master, just like the
# built-in mysqltest command save_master_pos.  The advantage of this
# script is that the saved position is available to the test script.
#
# The script will generate a channel name automatically if the
# $rpl_multi_source is set but no $rpl_channel_name is specified. The
# automatically generated channel name is based on the server_id of
# the master to sync with.
#
# ==== Usage ====
#
# [--let $use_gtids= 1]
# [--let $rpl_debug= 1]
# [--let $rpl_channel_name= 'NAME']
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
#   $rpl_channel_name
#     Uses this channel name rather than the default channel.
#     When $rpl_multi_source is set and $rpl_channel_name is not set this
#     include will guess the channel name based on master's server_id.
#
#   $rpl_debug
#     See include/rpl_init.inc


--let $include_filename= save_master_pos.inc
if ($rpl_channel_name)
{
  --let $include_filename= $include_filename [FOR CHANNEL $rpl_channel_name]
}
--source include/begin_include_file.inc

#
# Always save master positions, this will be useful to check missing GTIDs.
#
--let $_saved_file= query_get_value(SHOW MASTER STATUS, File, 1)
--let $_saved_pos= query_get_value(SHOW MASTER STATUS, Position, 1)
--let $_saved_channel_name= $rpl_channel_name
if ($rpl_multi_source)
{
  # If it was set to use multi source replication when calling rpl_init.inc
  # and without specifying the channel name, we will guess the channel name
  # to use based on the server_id of the master to sync with
  if (!$rpl_channel_name)
  {
    --let $_saved_channel_name= `SELECT CONCAT("'channel_", @@GLOBAL.SERVER_ID, "'")`
  }
}
if ($use_gtids)
{
  --source include/assert_gtid_mode_on.inc
  --let $_saved_gtids= `SELECT @@global.gtid_executed`
  if ($rpl_debug)
  {
    --echo save_master_pos saved gtid='$_saved_gtids', file='$_saved_file', pos='$_saved_pos' channel_name=$_saved_channel_name
  }
}
if (!$use_gtids)
{
  if ($rpl_debug)
  {
    --echo save_master_pos saved file='$_saved_file', pos='$_saved_pos' channel_name=$_saved_channel_name
  }
}

--let $include_filename= save_master_pos.inc
--source include/end_include_file.inc
