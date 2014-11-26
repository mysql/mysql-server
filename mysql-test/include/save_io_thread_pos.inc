# ==== Purpose ====
#
# Save the current position of the IO thread, just like
# save_master_pos.inc does for the master's position.
#
#
# ==== Usage ====
#
# [--let $use_gtids= 1]
# [--let $rpl_debug= 1]
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


--let $include_filename= save_io_thread_pos.inc
--source include/begin_include_file.inc

if ($use_gtids)
{
  --let $_saved_gtids= query_get_value(SHOW SLAVE STATUS, Retrieved_Gtid_Set, 1)
  if ($rpl_debug)
  {
    --echo save_io_thread_pos.inc saved gtid='$_saved_gtids'
  }
}
if (!$use_gtids)
{
  --let $_saved_file= query_get_value(SHOW SLAVE STATUS, Master_Log_File, 1)
  --let $_saved_pos= query_get_value(SHOW SLAVE STATUS, Read_Master_Log_Pos, 1)

  if ($rpl_debug)
  {
    --echo save_io_thread_pos.inc saved file='$_saved_file', pos='$_saved_pos'
  }
}

--let $include_filename= save_io_thread_pos.inc
--source include/end_include_file.inc
