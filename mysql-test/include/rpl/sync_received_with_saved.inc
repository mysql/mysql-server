# ==== Purpose ====
#
# Waits until the slave IO thread on the current connection has been
# synced up to the point saved by the last call to
# include/rpl/save_server_position.inc (i.e., until the IO thead has copied up
# to the saved position).  Does not wait for the SQL thread.
#
#
# ==== Usage ====
#
# [--let $use_gtids= 1]
# [--let $rpl_debug= 1]
# [--let $slave_timeout= NUMBER]
#
# On master:
# --source include/rpl/save_server_position.inc
#
# On slave:
# --source include/rpl/sync_received_with_saved.inc
#
# Parameters:
#   $use_gtids
#     If set, uses GTIDs instead of filename and offset for positions.
#
#   $slave_timeout
#     See include/rpl/wait_for_replica_status.inc
#
#   $rpl_debug
#     See include/rpl/init.inc
#
#   $ignore_gtids_on_sync
#     Forces the use of master file and position, even if $use_gtids is set.
#     This might be used if the slave will not have all the GTIDs of the master
#     but have to read and apply all master events to the end.


--let $include_filename= rpl/sync_received_with_saved.inc
--source include/begin_include_file.inc

--let $_old_slave_error_param= $slave_error_param
--let $slave_error_param= Last_IO_Errno

#
# Verify if the IO thread is already updated by the master position
# regardless the use of GTIDs. If the IO thread is already updated,
# we can reduce the slave timeout to a minimum because there in nothing
# else to sync.
# As there is a possibility of missing GTIDs on slave, this reduced
# timeout will make mtr throw an error almost promptly, without having
# to wait 300 seconds to notice a sync problem between master and slave.
#
--let $_slave_master_file= query_get_value(SHOW REPLICA STATUS, Source_Log_File, 1)
--let $_slave_master_pos= query_get_value(SHOW REPLICA STATUS, Read_Source_Log_Pos, 1)
--let $_saved_slave_timeout= $slave_timeout
if ( $_slave_master_file == $_saved_file )
{
  if ( $_slave_master_pos == $_saved_pos )
  {
    if ($rpl_debug)
    {
      --echo debug: It seems that IO thread is already synced (by verifying master file and position)
      --echo debug: Changing slave timeout to 1 second
    }
    --let $slave_timeout= 1
  }
}

--let $_sync_using_gtids= $use_gtids
if ($ignore_gtids_on_sync)
{
  --let $_sync_using_gtids= 0
}

if ($_sync_using_gtids)
{
  --source include/rpl/assert_gtid_mode_on.inc
  --let $slave_param= Retrieved_Gtid_Set
  --let $slave_param_value= $_saved_gtids
  --source include/rpl/wait_for_replica_status.inc
}
if (!$_sync_using_gtids)
{
  --let $slave_param= Source_Log_File
  --let $slave_param_value= $_saved_file
  --source include/rpl/wait_for_replica_status.inc

  --let $slave_param= Read_Source_Log_Pos
  --let $slave_param_value= $_saved_pos
  --source include/rpl/wait_for_replica_status.inc
}

--let $slave_error_param= $_old_slave_error_param

--let $slave_timeout= $_saved_slave_timeout

--let $include_filename= rpl/sync_received_with_saved.inc
--source include/end_include_file.inc
